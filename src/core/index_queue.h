#ifndef ANYTXT_INDEX_QUEUE_H
#define ANYTXT_INDEX_QUEUE_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <memory>

class XapianDatabase;
class XapianIndexer;
class ParserManager;

struct IndexResult {
    QString filePath;
    QString text;
    QMap<QString, QString> metadata;
    bool success = false;
    QString errorMessage;
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
    int maxWorkers() const { return m_maxWorkers; }
    void setMaxWorkers(int n) { m_maxWorkers = qMax(1, n); }
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
    void drainWriteQueue();
    bool isWorkDone();
    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<ParserManager> m_parser;
    QQueue<QPair<QString, int>> m_parseQueue;
    mutable QMutex m_parseMutex;
    QQueue<IndexResult> m_writeQueue;
    QMutex m_writeMutex;
    QWaitCondition m_writeCond;
    int m_activeParsers = 0;
    int m_maxWorkers = 4;
    volatile bool m_paused = false;
    volatile bool m_stopped = false;
    volatile bool m_writerRunning = false;
    int m_totalQueued = 0;
    int m_totalIndexed = 0;
    int m_totalFailed = 0;
    bool m_timingStarted = false;
    QElapsedTimer m_parseTime;
    QElapsedTimer m_writeTime;
};
#endif
