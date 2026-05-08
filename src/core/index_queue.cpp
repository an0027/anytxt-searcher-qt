#include <xapian.h>
#include "core/index_queue.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "parser/parser_manager.h"
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrent>
#include <QThreadPool>

IndexQueue::IndexQueue(std::shared_ptr<XapianDatabase> db,
                       std::shared_ptr<XapianIndexer> indexer,
                       std::shared_ptr<ParserManager> parser, QObject* parent)
    : QObject(parent), m_database(db), m_indexer(indexer), m_parser(parser)
{
    m_maxWorkers = qMax(2, QThread::idealThreadCount());
}

IndexQueue::~IndexQueue() { stop(); }

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

int IndexQueue::queueSize() const { QMutexLocker lock(&m_parseMutex); return m_parseQueue.size(); }
bool IndexQueue::isProcessing() const { QMutexLocker lock(&m_parseMutex); return m_activeParsers > 0 || !m_parseQueue.isEmpty() || m_writerRunning; }
void IndexQueue::clear() { QMutexLocker lock(&m_parseMutex); m_parseQueue.clear(); }
void IndexQueue::start() { m_paused = false; dispatchNext(); }
void IndexQueue::stop() { m_stopped = true; m_writeCond.wakeAll(); QThreadPool::globalInstance()->waitForDone(8000); }
void IndexQueue::pause() { m_paused = true; }
void IndexQueue::resume() { m_paused = false; dispatchNext(); }

bool IndexQueue::isWorkDone() { QMutexLocker lock(&m_parseMutex); return m_parseQueue.isEmpty() && m_activeParsers == 0; }

void IndexQueue::dispatchNext()
{
    if (!m_writerRunning) { m_writerRunning = true; QtConcurrent::run([this]() { drainWriteQueue(); }); }
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
        emit progressUpdated(m_totalIndexed + m_totalQueued - queueSize(), m_totalQueued, fi.fileName());
        QtConcurrent::run([this, filePath]() { parseFile(filePath); });
    }
}

void IndexQueue::parseFile(const QString& filePath)
{
    IndexResult r;
    r.filePath = filePath;
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
    { QMutexLocker lock(&m_writeMutex); m_writeQueue.enqueue(r); m_writeCond.wakeOne(); }
    QMetaObject::invokeMethod(this, [this, filePath, r]() {
        QMutexLocker lock(&m_parseMutex);
        m_activeParsers--;
        emit fileProcessed(filePath, r.success);
        if (m_parseQueue.isEmpty() && m_activeParsers == 0 && m_writeQueue.isEmpty() && !m_writerRunning) {
            emit queueEmpty();
            emit queueFinished(m_totalIndexed, m_totalFailed);
        }
        dispatchNext();
    }, Qt::QueuedConnection);
}

void IndexQueue::drainWriteQueue()
{
    while (!m_stopped) {
        IndexResult r;
        bool hasWork = false;
        {
            QMutexLocker lock(&m_writeMutex);
            if (!m_writeQueue.isEmpty()) { r = m_writeQueue.dequeue(); hasWork = true; }
            else if (isWorkDone()) {
                m_writerRunning = false;
                try { if (m_indexer) m_indexer->flush(); } catch (...) {}
                QMetaObject::invokeMethod(this, [this]() {
                    QMutexLocker lock(&m_parseMutex);
                    if (m_parseQueue.isEmpty() && m_activeParsers == 0) {
                        emit queueEmpty();
                        emit queueFinished(m_totalIndexed, m_totalFailed);
                    }
                }, Qt::QueuedConnection);
                return;
            } else { m_writeCond.wait(&m_writeMutex, 300); continue; }
        }
        if (hasWork) {
            try {
                if (r.success && m_indexer) m_indexer->addDocument(r.filePath, r.metadata, r.text);
                QMetaObject::invokeMethod(this, [this, r]() {
                    QMutexLocker lock(&m_parseMutex);
                    if (r.success) { m_totalIndexed++; }
                    else { m_totalFailed++; emit logMessage(QStringLiteral("x %1 - %2").arg(QFileInfo(r.filePath).fileName(), r.errorMessage)); }
                }, Qt::QueuedConnection);
            } catch (const std::exception& e) {
                QMetaObject::invokeMethod(this, [this, fp = r.filePath, e]() {
                    QMutexLocker lock(&m_parseMutex);
                    m_totalFailed++;
                    emit logMessage(QStringLiteral("x %1").arg(QFileInfo(fp).fileName()));
                }, Qt::QueuedConnection);
            }
        }
    }
    m_writerRunning = false;
}
