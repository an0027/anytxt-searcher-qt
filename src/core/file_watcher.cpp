/*
 * file_watcher.cpp - 文件系统监听器实现

实现递归文件夹监听、文件变更检测、防抖合并通知、
已知文件状态跟踪和变更差异计算。
 */

#include "core/file_watcher.h"
#include <QDir>
#include <QFileInfo>
#include <QDirIterator>
#include <QDebug>

FileWatcher::FileWatcher(QObject* parent)
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
    m_debounceTimer = new QTimer(this);
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(m_debounceMs);

    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &FileWatcher::onDirectoryChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &FileWatcher::onFileChanged);
    connect(m_debounceTimer, &QTimer::timeout,
            this, &FileWatcher::processChanges);
}

FileWatcher::~FileWatcher()
{
    stop();
}

void FileWatcher::setWatchInterval(int ms)
{
    m_debounceMs = qMax(500, ms);
    m_debounceTimer->setInterval(m_debounceMs);
}

void FileWatcher::addWatchPath(const QString& path)
{
    QFileInfo fi(path);
    if (!fi.exists()) return;

    QString absPath = fi.absoluteFilePath();

    if (fi.isDir()) {
        addRecursivePaths(absPath);
        scanDirectory(absPath, m_fileTimestamps);
    } else {
        if (!m_watcher->files().contains(absPath)) {
            m_watcher->addPath(absPath);
        }
        m_fileTimestamps[absPath] = fi.lastModified();
    }

    qDebug() << "FileWatcher: added path" << absPath;
}

void FileWatcher::addRecursivePaths(const QString& dirPath)
{
    if (!m_watcher->directories().contains(dirPath)) {
        m_watcher->addPath(dirPath);
    }

    QDirIterator it(dirPath, QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString subDir = it.next();
        if (!m_watcher->directories().contains(subDir)) {
            m_watcher->addPath(subDir);
        }
    }
}

void FileWatcher::removeWatchPath(const QString& path)
{
    // Remove directory and all subdirectories from watcher
    QStringList dirs = m_watcher->directories();
    for (const auto& d : dirs) {
        if (d.startsWith(path)) {
            m_watcher->removePath(d);
        }
    }

    // Remove files
    QStringList files = m_watcher->files();
    for (const auto& f : files) {
        if (f.startsWith(path)) {
            m_watcher->removePath(f);
        }
    }

    // Clean known data
    auto it = m_fileTimestamps.begin();
    while (it != m_fileTimestamps.end()) {
        if (it.key().startsWith(path)) {
            m_knownFiles.remove(it.key());
            it = m_fileTimestamps.erase(it);
        } else {
            ++it;
        }
    }

    qDebug() << "FileWatcher: removed path" << path;
}

QStringList FileWatcher::watchedPaths() const
{
    return m_watcher->directories() + m_watcher->files();
}

void FileWatcher::start()
{
    if (m_watching) return;
    m_watching = true;
    qDebug() << "FileWatcher: started";
}

void FileWatcher::stop()
{
    if (!m_watching) return;
    m_watching = false;
    m_debounceTimer->stop();
    qDebug() << "FileWatcher: stopped";
}

void FileWatcher::scanNow()
{
    if (!m_watching) return;
    onDirectoryChanged(QString());
}

void FileWatcher::onDirectoryChanged(const QString& path)
{
    if (!m_watching) return;

    Q_UNUSED(path);
    // Debounce: restart timer on each change
    m_debounceTimer->start();
}

void FileWatcher::onFileChanged(const QString& path)
{
    if (!m_watching) return;

    QFileInfo fi(path);
    if (fi.exists()) {
        QDateTime newMtime = fi.lastModified();
        auto it = m_fileTimestamps.find(path);
        if (it != m_fileTimestamps.end() && it.value() < newMtime) {
            m_pendingModified.append(path);
            it.value() = newMtime;
        } else if (it == m_fileTimestamps.end()) {
            m_pendingNew.append(path);
            m_fileTimestamps[path] = newMtime;
        }
    } else {
        m_pendingDeleted.append(path);
        m_fileTimestamps.remove(path);
        m_knownFiles.remove(path);
    }

    m_debounceTimer->start();
}

void FileWatcher::processChanges()
{
    if (!m_watching) return;

    // Scan all watched directories for new/modified files
    QStringList dirs = m_watcher->directories();

    // Collect current file state
    QMap<QString, QDateTime> currentFiles;
    for (const auto& dir : dirs) {
        scanDirectory(dir, currentFiles);
    }

    // Detect new files
    QStringList newFiles;
    for (auto it = currentFiles.begin(); it != currentFiles.end(); ++it) {
        if (!m_knownFiles.contains(it.key())) {
            // Check if it's a supported file
            QString ext = QFileInfo(it.key()).suffix().toLower();
            QStringList supportedExts = {
                "txt", "md", "pdf", "docx", "doc", "rtf",
                "png", "jpg", "jpeg", "bmp", "gif", "tiff",
                "csv", "xml", "json", "html", "htm",
                "c", "cpp", "h", "hpp", "py", "js", "ts",
                "java", "rs", "go", "rb", "php", "sh"
            };
            if (supportedExts.contains(ext)) {
                newFiles.append(it.key());
            }
        }
    }

    // Detect deleted files
    QStringList deletedFiles;
    for (const auto& known : m_knownFiles) {
        if (!currentFiles.contains(known) && QFileInfo::exists(known)) {
            // File was deleted
            deletedFiles.append(known);
        }
    }

    m_pendingNew.clear();
    m_pendingModified.clear();
    m_pendingDeleted.clear();

    m_knownFiles = QSet<QString>(currentFiles.keyBegin(), currentFiles.keyEnd());
    m_fileTimestamps = currentFiles;

    if (!newFiles.isEmpty() || !deletedFiles.isEmpty()) {
        qDebug() << "FileWatcher: detected" << newFiles.size()
                 << "new," << deletedFiles.size() << "deleted files";
    }

    if (!newFiles.isEmpty()) {
        emit filesChanged(newFiles, {}, deletedFiles);
    }
}

void FileWatcher::scanDirectory(const QString& path, QMap<QString, QDateTime>& files)
{
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fi(filePath);
        files[fi.absoluteFilePath()] = fi.lastModified();
    }
}
