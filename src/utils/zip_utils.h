/*
 * zip_utils.h - ZIP 工具函数（Windows Unicode 路径支持）
 *
 * libzip 的 zip_open() 在 Windows 上不支持 UTF-8 路径名。
 * 此工具提供 zip_open_utf8() 包装，自动选择合适 API。
 */

#ifndef ANYTXT_ZIP_UTILS_H
#define ANYTXT_ZIP_UTILS_H

#include <zip.h>
#include <QString>

#ifdef Q_OS_WIN
#include <windows.h>

/**
 * @brief 以 UTF-8 路径打开 ZIP 文件（Windows 专用）
 *
 * Windows 上使用 zip_open_w() 支持 Unicode 路径名。
 * 非 Windows 平台直接使用 zip_open()。
 */
inline zip_t* zip_open_utf8(const QString& zipPath, int flags, int* errorCode)
{
#ifdef HAS_LIBZIP_WIDE_API
    // Use wide-char API if available (libzip >= 1.7)
    std::wstring wpath = zipPath.toStdWString();
    return zip_open_w(wpath.c_str(), flags, errorCode);
#else
    // Fallback: convert to system locale ANSI
    // QFile::encodeName() uses the system encoding on Windows
    QByteArray ansiPath = QFile::encodeName(zipPath);
    return zip_open(ansiPath.constData(), flags, errorCode);
#endif
}

#else
// Non-Windows: use UTF-8 directly
inline zip_t* zip_open_utf8(const QString& zipPath, int flags, int* errorCode)
{
    return zip_open(zipPath.toUtf8().constData(), flags, errorCode);
}
#endif

#endif // ANYTXT_ZIP_UTILS_H
