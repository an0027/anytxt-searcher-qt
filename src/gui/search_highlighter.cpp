/*
 * search_highlighter.cpp - 搜索关键词高亮器实现
 */

#include "search_highlighter.h"

SearchHighlighter::SearchHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent)
{
}

void SearchHighlighter::setKeywords(const QString& query)
{
    m_rules.clear();

    if (query.trimmed().isEmpty()) return;

    // Remove operators and split into words
    QString clean = query;
    clean.remove(QRegularExpression(QStringLiteral("[&|\\-\\\"\\(\\)]")));
    QStringList words = clean.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

    QTextCharFormat fmt;
    fmt.setBackground(QColor("#2B5B84"));
    fmt.setForeground(QColor("#E8E8E8"));

    for (const auto& word : words) {
        Rule rule;
        rule.pattern = QRegularExpression(
            QRegularExpression::escape(word),
            QRegularExpression::CaseInsensitiveOption);
        rule.format = fmt;
        m_rules.append(rule);
    }

    // Force rehighlight
    rehighlight();
}

void SearchHighlighter::clear()
{
    m_rules.clear();
    rehighlight();
}

void SearchHighlighter::highlightBlock(const QString& text)
{
    for (const auto& rule : m_rules) {
        QRegularExpressionMatchIterator it = rule.pattern.globalMatch(text);
        while (it.hasNext()) {
            QRegularExpressionMatch match = it.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}
