/*
 * index_queue.h - 索引队列管理器

功能说明：后台索引队列，接收文件监听器传来的索引任务，
在后台线程中依次处理。支持批量入队、暂停/恢复、
进度报告和线程安全操作。
 */

#ifndef ANYTXT_INDEX_QUEUE_H
#define ANYTXT_INDEX_QUEUE_H

#include <QObject>
#include <QStringList>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QAtomicInt>
#include <memory>

class XapianDatabase;
class XapianIndexer;
class ParserManager;

class IndexQueueWorker : public QObject {
    Q_OBJECT
public:
    IndexQueueWorker(std::shared_ptr<XapianDatabase> db,
                     std::shared_ptr<XapianIndexer> indexer,
                     std::shared_ptr<ParserManager> parser);
    ~IndexQueueWorker() override;

public slots:
    void process();

signals:
    void progressUpdated(int indexed, int total, const QString& currentFile);
    void finished(int indexed, int failed);
    void fileIndexed(const QString& filePath, bool success);
    void logMessage(const QString& msg);

private:
    bool indexFile(const QString& path);

    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<ParserManager> m_parser;
    QMutex m_mutex;
};

class IndexQueue : public QObject {
    Q_OBJECT
public:
    explicit IndexQueue(std::shared_ptr<XapianDatabase> db,
                        std::shared_ptr<XapianIndexer> indexer,
                        std::shared_ptr<ParserManager> parser,
                        QObject* parent = nullptr);
    ~IndexQueue() override;

    void enqueue(const QString& filePath, int priority = 0);
    void enqueueBatch(const QStringList& filePaths, int priority = 0);
    int queueSize() const;
    bool isProcessing() const;
    void clear();

signals:
    void progressUpdated(int indexed, int total, const QString& currentFile);
    void queueFinished(int indexed, int failed);
    void fileProcessed(const QString& filePath, bool success);
    void logMessage(const QString& msg);
    void queueEmpty();

public slots:
    void start();
    void stop();
    void pause();
    void resume();

private:
    void processNext();

    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<ParserManager> m_parser;

    QThread* m_workerThread;
    IndexQueueWorker* m_worker;
    QQueue<QPair<QString, int>> m_queue;
    mutable QMutex m_mutex;
    bool m_processing = false;
    bool m_paused = false;
    int m_totalQueued = 0;
    int m_totalIndexed = 0;
    int m_totalFailed = 0;
};

#endif // ANYTXT_INDEX_QUEUE_H
