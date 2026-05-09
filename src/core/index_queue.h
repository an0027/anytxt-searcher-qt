#ifndef ANYTXT_INDEX_QUEUE_H
#define ANYTXT_INDEX_QUEUE_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QQueue>
#include <QVector>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <memory>
#include <atomic>

class XapianDatabase;
class XapianIndexer;
class ParserManager;

struct IndexResult {
    QString filePath;
    QString text;
    QMap<QString, QString> metadata;
    bool success = false;
    QString errorMessage;
    int shardIndex = 0;  ///< 目标分片索引
};

class IndexQueue : public QObject {
    Q_OBJECT
public:
    explicit IndexQueue(
        const QVector<std::shared_ptr<XapianDatabase>>& databases,
        const QVector<std::shared_ptr<XapianIndexer>>& indexers,
        std::shared_ptr<ParserManager> parser,
        QObject* parent = nullptr);
    ~IndexQueue() override;

    void enqueue(const QString& filePath, int priority = 0);
    void enqueueBatch(const QStringList& filePaths, int priority = 0);

    int queueSize() const;
    bool isProcessing() const;
    void clear();

    int maxWorkers() const { return m_maxWorkers; }
    void setMaxWorkers(int n) { m_maxWorkers = qMax(1, n); }
    int shardCount() const { return m_shardCount; }

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
    void dispatchNext();
    void parseFile(const QString& filePath);
    void drainWriteQueue(int shardIndex);
    bool isWorkDone();

    // One parse queue for all files
    QQueue<QPair<QString, int>> m_parseQueue;
    mutable QMutex m_parseMutex;

    // N write queues, one per shard (use pointers for non-copyable types)
    QVector<QQueue<IndexResult>> m_writeQueues;
    QVector<QMutex*> m_writeMutexes;
    QVector<QWaitCondition*> m_writeConds;
    QVector<int> m_writerRunning; // 0=stopped, 1=running (int for QList compat)

    // Per-shard resources
    QVector<std::shared_ptr<XapianDatabase>> m_databases;
    QVector<std::shared_ptr<XapianIndexer>> m_indexers;

    std::shared_ptr<ParserManager> m_parser;
    int m_shardCount = 4;
    int m_activeParsers = 0;
    int m_maxWorkers = 4;
    int m_totalQueued = 0;
    int m_totalIndexed = 0;
    int m_totalFailed = 0;
    std::atomic<bool> m_stopped{false};
    std::atomic<bool> m_paused{false};
    bool m_timingStarted = false;
    QElapsedTimer m_parseTime;
    QElapsedTimer m_writeTime;
};

#endif // ANYTXT_INDEX_QUEUE_H
