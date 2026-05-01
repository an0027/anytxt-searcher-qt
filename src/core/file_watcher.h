/*
 * file_watcher.h - 文件系统监听器

功能说明：基于 QFileSystemWatcher 的文件夹监听模块。
自动检测文件夹中新增、修改、删除的文件，支持递归监听、
防抖处理和文件类型过滤，用于实现智能自动索引。
 */

#ifndef ANYTXT_FILE_WATCHER_H
#define ANYTXT_FILE_WATCHER_H

#include <QObject>
#include <QFileSystemWatcher>
#include <QStringList>
#include <QSet>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <memory>

class FileWatcher : public QObject {
    Q_OBJECT
public:
    explicit FileWatcher(QObject* parent = nullptr);
    ~FileWatcher() override;

    void addWatchPath(const QString& path);
    void removeWatchPath(const QString& path);
    QStringList watchedPaths() const;
    void setWatchInterval(int ms);

signals:
    void filesChanged(const QStringList& newFiles, const QStringList& modifiedFiles,
                      const QStringList& deletedFiles);
    void directoryChanged(const QString& path);

public slots:
    void start();
    void stop();
    bool isWatching() const { return m_watching; }
    void scanNow();

private slots:
    void onDirectoryChanged(const QString& path);
    void onFileChanged(const QString& path);
    void processChanges();

private:
    void scanDirectory(const QString& path, QMap<QString, QDateTime>& files);
    void addRecursivePaths(const QString& dirPath);

    QFileSystemWatcher* m_watcher;
    QTimer* m_debounceTimer;
    QSet<QString> m_knownFiles;
    QMap<QString, QDateTime> m_fileTimestamps;
    QStringList m_pendingNew;
    QStringList m_pendingModified;
    QStringList m_pendingDeleted;
    bool m_watching = false;
    int m_debounceMs = 2000;
};

#endif // ANYTXT_FILE_WATCHER_H
