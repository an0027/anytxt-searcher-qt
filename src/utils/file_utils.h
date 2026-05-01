/*
 * file_utils.h - 文件工具函数

功能说明：提供文件大小格式化、MIME 类型检测、
文件图标获取等通用工具函数。
 */

#ifndef ANYTXT_FILE_UTILS_H
#define ANYTXT_FILE_UTILS_H

#include <QString>
#include <QIcon>

class FileUtils {
public:
    static QString formatFileSize(int64_t bytes);
    static QString detectMimeType(const QString& filePath);
    static QString sanitizeTerm(const QString& term);
    static QIcon getFileIcon(const QString& filePath);
};

#endif // ANYTXT_FILE_UTILS_H
