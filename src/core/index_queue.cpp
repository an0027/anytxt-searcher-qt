#include <xapian.h>
#include "core/index_queue.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "parser/parser_manager.h"
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrent>
#include <QThreadPool>

IndexQueue::IndexQueue(
    const QVector<std::shared_ptr<XapianDatabase>>& databases,
    const QVector<std::shared_ptr<XapianIndexer>>& indexers,
    std::shared_ptr<ParserManager> parser, QObject* parent)
    : QObject(parent)
    , m_databases(databases)
    , m_indexers(indexers)
    , m_parser(parser)
{
    m_shardCount = qMax(1, databases.size());
    m_maxWorkers = qMax(2, QThread::idealThreadCount());

    // Initialize per-shard queues, mutexes, conditions
    m_writeQueues.resize(m_shardCount);
    m_writeMutexes.resize(m_shardCount);
    m_writeConds.resize(m_shardCount);
    m_writerRunning.resize(m_shardCount);
    for (int i = 0; i < m_shardCount; ++i) {
        m_writeMutexes[i] = new QMutex();
        m_writeConds[i] = new QWaitCondition();
        m_writerRunning[i] = false;
    }
}

IndexQueue::~IndexQueue()
{
    stop();
    for (auto* m : m_writeMutexes) delete m;
    for (auto* c : m_writeConds) delete c;
}

static int shardForPath(const QString& filePath, int shardCount)
{
    return qHash(filePath) % shardCount;
}

void IndexQueue::enqueue(const QString& filePath, int priority)
{
    QMutexLocker lock(&m_parseMutex);
    m_parseQueue.enqueue({filePath, priority});
    m_totalQueued++;
}

void IndexQueue::enqueueBatch(const QStringList& filePaths, int priority)
{
    {
        QMutexLocker lock(&m_parseMutex);
        for (const auto& p : filePaths) { m_parseQueue.enqueue({p, priority}); }
        m_totalQueued += filePaths.size();
        if (!m_timingStarted) { m_parseTime.start(); m_writeTime.start(); m_timingStarted = true; }
    }
    dispatchNext();
}

int IndexQueue::queueSize() const
{
    QMutexLocker lock(&m_parseMutex);
    return m_parseQueue.size();
}

bool IndexQueue::isProcessing() const
{
    QMutexLocker lock(&m_parseMutex);
    if (m_activeParsers > 0 || !m_parseQueue.isEmpty()) return true;
    for (int i = 0; i < m_shardCount; ++i) {
        if (m_writerRunning[i] || !m_writeQueues[i].isEmpty()) return true;
    }
    return false;
}

void IndexQueue::clear()
{
    QMutexLocker lock(&m_parseMutex);
    m_parseQueue.clear();
}

void IndexQueue::start()
{
    m_paused = false;
    dispatchNext();
}

void IndexQueue::stop()
{
    m_stopped = true;
    for (int i = 0; i < m_shardCount; ++i) {
        if (m_writeConds[i]) m_writeConds[i]->wakeAll();
    }
    QThreadPool::globalInstance()->waitForDone(8000);
}

void IndexQueue::pause() { m_paused = true; }
void IndexQueue::resume() { m_paused = false; dispatchNext(); }

bool IndexQueue::isWorkDone()
{
    QMutexLocker lock(&m_parseMutex);
    return m_parseQueue.isEmpty() && m_activeParsers == 0;
}

void IndexQueue::dispatchNext()
{
    // Start writer threads for each shard (one thread per shard, runs until done)
    for (int i = 0; i < m_shardCount; ++i) {
        if (!m_writerRunning[i]) {
            m_writerRunning[i] = true;
            QtConcurrent::run([this, i]() { drainWriteQueue(i); });
        }
    }

    // Dispatch parse tasks
    while (true) {
        QString filePath;
        {
            QMutexLocker lock(&m_parseMutex);
            if (m_stopped || m_paused || m_parseQueue.isEmpty()) return;
            if (m_activeParsers >= m_maxWorkers) return;
            filePath = m_parseQueue.dequeue().first;
            m_activeParsers++;
        }
        QFileInfo fi(filePath);
        emit progressUpdated(m_totalIndexed + m_totalQueued - queueSize(),
                             m_totalQueued, fi.fileName());
        QtConcurrent::run([this, filePath]() { parseFile(filePath); });
    }
}

void IndexQueue::parseFile(const QString& filePath)
{
    IndexResult r;
    r.filePath = filePath;
    r.shardIndex = shardForPath(filePath, m_shardCount);

    try {
        auto res = m_parser->processDocument(filePath);
        r.success = res.success;
        r.text = res.text;
        r.metadata = res.metadata;
        r.errorMessage = res.errorMessage;
    } catch (const std::exception& e) {
        r.success = false;
        r.errorMessage = QString::fromLatin1(e.what());
    }

    // Enqueue to the correct shard's write queue
    {
        QMutexLocker lock(m_writeMutexes[r.shardIndex]);
        m_writeQueues[r.shardIndex].enqueue(r);
        if (m_writeConds[r.shardIndex]) m_writeConds[r.shardIndex]->wakeOne();
    }

    QMetaObject::invokeMethod(this, [this, filePath, r]() {
        QMutexLocker lock(&m_parseMutex);
        m_activeParsers--;
        emit fileProcessed(filePath, r.success);

        // Check if everything is done
        bool allDone = m_parseQueue.isEmpty() && m_activeParsers == 0;
        for (int i = 0; i < m_shardCount && allDone; ++i) {
            allDone = allDone && m_writeQueues[i].isEmpty() && !m_writerRunning[i];
        }
        if (allDone) {
            emit queueEmpty();
            emit queueFinished(m_totalIndexed, m_totalFailed);
        }

        dispatchNext();
    }, Qt::QueuedConnection);
}

void IndexQueue::drainWriteQueue(int shardIndex)
{
    const int batchSize = (shardIndex < m_indexers.size() && m_indexers[shardIndex])
                          ? m_indexers[shardIndex]->batchSize() : 2000;
    auto& idxr = m_indexers[shardIndex];
    int docsInTxn = 0;

    QQueue<IndexResult>& writeQueue = m_writeQueues[shardIndex];
    QMutex* writeMutex = m_writeMutexes[shardIndex];
    QWaitCondition* writeCond = m_writeConds[shardIndex];

    while (!m_stopped) {
        IndexResult r;
        bool hasWork = false;
        {
            QMutexLocker lock(writeMutex);
            if (!writeQueue.isEmpty()) { r = writeQueue.dequeue(); hasWork = true; }
            else if (isWorkDone()) {
                // Commit last batch
                if (docsInTxn > 0 && idxr) {
                    try { idxr->commitBatch(); } catch (...) {}
                }
                docsInTxn = 0;
                m_writerRunning[shardIndex] = false;
                if (idxr) { try { idxr->flush(); } catch (...) {} }
                return;
            } else {
                writeCond->wait(writeMutex);
                continue;
            }
        }

        if (hasWork) {
            try {
                if (r.success && idxr) {
                    if (docsInTxn == 0) idxr->beginBatch();
                    idxr->addDocument(r.filePath, r.metadata, r.text);
                    docsInTxn++;
                    if (docsInTxn >= batchSize) {
                        idxr->commitBatch();
                        docsInTxn = 0;
                    }
                }
                QMetaObject::invokeMethod(this, [this, r]() {
                    QMutexLocker lock(&m_parseMutex);
                    if (r.success) { m_totalIndexed++; }
                    else { m_totalFailed++;
                        emit logMessage(QStringLiteral("x %1 - %2")
                            .arg(QFileInfo(r.filePath).fileName(), r.errorMessage)); }
                }, Qt::QueuedConnection);
            } catch (const std::exception& e) {
                if (docsInTxn > 0 && idxr) {
                    try { idxr->commitBatch(); } catch (...) {}
                    docsInTxn = 0;
                }
                QMetaObject::invokeMethod(this, [this, fp = r.filePath]() {
                    QMutexLocker lock(&m_parseMutex);
                    m_totalFailed++;
                    emit logMessage(QStringLiteral("x %1").arg(QFileInfo(fp).fileName()));
                }, Qt::QueuedConnection);
            }
        }
    }

    // Stopped: commit pending transaction
    if (docsInTxn > 0 && idxr) {
        try { idxr->commitBatch(); } catch (...) {}
    }
    m_writerRunning[shardIndex] = false;
}
