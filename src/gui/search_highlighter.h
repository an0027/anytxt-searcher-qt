/*
 * search_highlighter.h - 搜索关键词高亮器
 */

#ifndef ANYTXT_SEARCH_HIGHLIGHTER_H
#define ANYTXT_SEARCH_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextDocument>
#include <QRegularExpression>
#include <QVector>

class SearchHighlighter : public QSyntaxHighlighter {
    Q_OBJECT
public:
    explicit SearchHighlighter(QTextDocument* parent = nullptr);

    void setKeywords(const QString& query);
    void clear();

protected:
    void highlightBlock(const QString& text) override;

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<Rule> m_rules;
};

#endif
