/*
 * string_utils.cpp - 字符串工具函数实现

实现字符串处理的工具函数。
 */

#include "utils/string_utils.h"
#include <QRegularExpression>
#include <algorithm>
#include <QDebug>

QString StringUtils::escapeHtml(const QString& text)
{
    QString result = text;
    result.replace('&', "&amp;");
    result.replace('<', "&lt;");
    result.replace('>', "&gt;");
    result.replace('"', "&quot;");
    result.replace('\'', "&#39;");
    return result;
}

QString StringUtils::highlightText(const QString& text, const QString& query)
{
    if (query.trimmed().isEmpty() || text.isEmpty()) {
        return escapeHtml(text);
    }

    QString escaped = escapeHtml(text);
    QStringList keywords = extractKeywords(query);

    for (const auto& keyword : keywords) {
        if (keyword.length() < 1) continue;

        QString escapedKeyword = escapeHtml(keyword);
        // Case-insensitive highlighting
        QString pattern = QString("(%1)").arg(QRegularExpression::escape(escapedKeyword));

        QRegularExpression re(pattern,
            QRegularExpression::CaseInsensitiveOption |
            QRegularExpression::UseUnicodePropertiesOption);

        escaped.replace(re, "<span style=\"background-color: #4FC3F7; color: #000; font-weight: bold;\">\\1</span>");
    }

    return escaped;
}

QString StringUtils::truncateText(const QString& text, int maxLen)
{
    if (text.length() <= maxLen) {
        return text;
    }

    QString truncated = text.left(maxLen - 3);
    // Try to break at a word boundary
    int lastSpace = truncated.lastIndexOf(' ');
    if (lastSpace > maxLen / 2) {
        truncated = truncated.left(lastSpace);
    }

    return truncated + "...";
}

bool StringUtils::isChinese(const QString& text)
{
    for (const QChar& ch : text) {
        uint codepoint = ch.unicode();
        // CJK Unified Ideographs range
        if (codepoint >= 0x4E00 && codepoint <= 0x9FFF) {
            return true;
        }
        // CJK Unified Ideographs Extension A
        if (codepoint >= 0x3400 && codepoint <= 0x4DBF) {
            return true;
        }
        // CJK Unified Ideographs Extension B
        if (codepoint >= 0x20000 && codepoint <= 0x2A6DF) {
            return true;
        }
        // CJK Compatibility Ideographs
        if (codepoint >= 0xF900 && codepoint <= 0xFAFF) {
            return true;
        }
    }
    return false;
}

QStringList StringUtils::extractKeywords(const QString& query)
{
    // Remove boolean operators and extract words
    QString cleaned = query;
    // Remove common operators
    QStringList operators = {"AND", "OR", "NOT", "title:", "file:"};
    for (const auto& op : operators) {
        cleaned.replace(op, " ", Qt::CaseInsensitive);
    }

    QStringList words = cleaned.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    // Remove short words and duplicates
    QStringList keywords;
    QSet<QString> seen;
    for (const auto& word : words) {
        QString lower = word.toLower();
        if (word.length() >= 2 && !seen.contains(lower)) {
            keywords.append(word);
            seen.insert(lower);
        }
    }

    return keywords;
}
