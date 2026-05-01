// Must include xapian before Qt to avoid keyword clashes
#include <xapian.h>
#include "core/index_queue.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "parser/parser_manager.h"
#include <QFileInfo>
#include <QTimer>
#include <QtConcurrent>
#include <QDebug>

// ====== IndexQueueWorker ======

IndexQueueWorker::IndexQueueWorker(std::shared_ptr<XapianDatabase> db,
                                   std::shared_ptr<XapianIndexer> indexer,
                                   std::shared_ptr<ParserManager> parser)
    : m_database(db), m_indexer(indexer), m_parser(parser)
{
}

IndexQueueWorker::~IndexQueueWorker()
{
}

bool IndexQueueWorker::indexFile(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists() || !fi.isReadable()) {
        emit logMessage(QString("无法读取文件: %1").arg(path));
        return false;
    }

    emit fileIndexed(path, false); // placeholder - will be updated
    emit logMessage(QString("索引: %1").arg(fi.fileName()));

    try {
        auto result = m_parser->processDocument(path);
        if (!result.success) {
            emit logMessage(QString("解析失败: %1 - %2")
                .arg(fi.fileName(), result.errorMessage));
            return false;
        }

        QMap<QString, QString> meta = result.metadata;
        int64_t docId = m_indexer->addDocument(path, meta, result.text);
        Q_UNUSED(docId);

        emit logMessage(QString("✓ 已完成: %1").arg(fi.fileName()));
        return true;

    } catch (const std::exception& e) {
        emit logMessage(QString("✗ 索引失败: %1 - %2")
            .arg(fi.fileName(), e.what()));
        return false;
    }
}

void IndexQueueWorker::process()
{
    // This is called from the thread - the actual queue management
    // is done in the main thread. This worker handles one-shots.
}

// ====== IndexQueue ======

IndexQueue::IndexQueue(std::shared_ptr<XapianDatabase> db,
                       std::shared_ptr<XapianIndexer> indexer,
                       std::shared_ptr<ParserManager> parser,
                       QObject* parent)
    : QObject(parent),
      m_database(db), m_indexer(indexer), m_parser(parser)
{
    m_workerThread = new QThread(this);
    m_worker = new IndexQueueWorker(db, indexer, parser);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &IndexQueueWorker::process);
    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_worker, &IndexQueueWorker::logMessage, this, &IndexQueue::logMessage);
}

IndexQueue::~IndexQueue()
{
    stop();
}

void IndexQueue::enqueue(const QString& filePath, int priority)
{
    QMutexLocker lock(&m_mutex);
    m_queue.enqueue({filePath, priority});
    m_totalQueued++;
}

void IndexQueue::enqueueBatch(const QStringList& filePaths, int priority)
{
    QMutexLocker lock(&m_mutex);
    for (const auto& path : filePaths) {
        m_queue.enqueue({path, priority});
    }
    m_totalQueued += filePaths.size();

    if (!m_processing && !m_paused) {
        processNext();
    }
}

int IndexQueue::queueSize() const
{
    QMutexLocker lock(&m_mutex);
    return m_queue.size();
}

bool IndexQueue::isProcessing() const
{
    return m_processing;
}

void IndexQueue::clear()
{
    QMutexLocker lock(&m_mutex);
    m_queue.clear();
}

void IndexQueue::start()
{
    if (!m_workerThread->isRunning()) {
        m_workerThread->start();
    }
    m_paused = false;
    if (!m_queue.isEmpty() && !m_processing) {
        processNext();
    }
}

void IndexQueue::stop()
{
    if (m_workerThread->isRunning()) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    m_processing = false;
}

void IndexQueue::pause()
{
    m_paused = true;
}

void IndexQueue::resume()
{
    m_paused = false;
    if (!m_queue.isEmpty() && !m_processing) {
        processNext();
    }
}

void IndexQueue::processNext()
{
    QString filePath;
    int currentIndexed, currentQueued;
    {
        QMutexLocker lock(&m_mutex);
        if (m_queue.isEmpty() || m_paused) {
            if (m_queue.isEmpty()) {
                m_processing = false;
                emit queueEmpty();
                emit queueFinished(m_totalIndexed, m_totalFailed);
            }
            return;
        }

        m_processing = true;
        auto [fp, priority] = m_queue.dequeue();
        Q_UNUSED(priority);
        filePath = fp;
        currentIndexed = m_totalIndexed;
        currentQueued = m_totalQueued;
    } // mutex released

    QFileInfo fi(filePath);
    emit progressUpdated(currentIndexed + 1, currentQueued, fi.fileName());

    // Run parsing and indexing in background thread
    auto db = m_database;
    auto indexer = m_indexer;
    auto parser = m_parser;
    QtConcurrent::run([db, indexer, parser, filePath, this]() {
        bool success = false;
        try {
            auto result = parser->processDocument(filePath);
            if (result.success) {
                indexer->addDocument(filePath, result.metadata, result.text);
                success = true;
            }
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, filePath, e]() {
                emit logMessage(QString("✗ 索引失败: %1 - %2")
                    .arg(QFileInfo(filePath).fileName(), e.what()));
            }, Qt::QueuedConnection);
        }

        // Update counters and emit completion on main thread
        QMetaObject::invokeMethod(this, [this, filePath, success]() {
            QMutexLocker lock(&m_mutex);
            if (success) {
                m_totalIndexed++;
                emit logMessage(QString("✓ 索引完成: %1")
                    .arg(QFileInfo(filePath).fileName()));
            } else {
                m_totalFailed++;
            }
            emit fileProcessed(filePath, success);
            m_processing = false;
            lock.unlock();

            // Schedule next item
            QTimer::singleShot(0, this, [this]() {
                processNext();
            });
        }, Qt::QueuedConnection);
    });
}
