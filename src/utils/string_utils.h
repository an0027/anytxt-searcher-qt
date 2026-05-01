/*
 * string_utils.h - 字符串工具函数

功能说明：提供搜索关键词高亮、中文检测、
文本截断等字符串处理工具。
 */

#ifndef ANYTXT_STRING_UTILS_H
#define ANYTXT_STRING_UTILS_H

#include <QString>
#include <QStringList>

class StringUtils {
public:
    static QString highlightText(const QString& text, const QString& query);
    static QString truncateText(const QString& text, int maxLen);
    static bool isChinese(const QString& text);
    static QStringList extractKeywords(const QString& query);

private:
    static QString escapeHtml(const QString& text);
};

#endif // ANYTXT_STRING_UTILS_H
