/*
 * file_utils.cpp - 文件工具函数实现

实现文件操作相关的工具函数。
 */

#include "utils/file_utils.h"
#include <QMimeDatabase>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QDebug>

static QMimeDatabase s_mimeDb;

QString FileUtils::formatFileSize(int64_t bytes)
{
    if (bytes < 0) return "0 B";

    static const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < 4) {
        size /= 1024.0;
        unitIndex++;
    }

    if (unitIndex == 0) {
        return QString("%1 B").arg(bytes);
    }

    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

QString FileUtils::detectMimeType(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        return "application/octet-stream";
    }

    QMimeType mimeType = s_mimeDb.mimeTypeForFile(filePath);
    return mimeType.name();
}

QString FileUtils::sanitizeTerm(const QString& term)
{
    QString sanitized;
    sanitized.reserve(term.size());

    for (const QChar& ch : term) {
        if (ch.isLetterOrNumber() || ch.isSpace() || ch == '-' || ch == '_') {
            sanitized.append(ch);
        }
    }

    return sanitized.trimmed();
}

QIcon FileUtils::getFileIcon(const QString& filePath)
{
    QFileIconProvider provider;
    QFileInfo fi(filePath);
    // Also works for non-existent files based on extension
    return provider.icon(fi);
}
