/*
 * xapian_searcher.h - Xapian 搜索器封装

功能说明：封装 Xapian::Enquire 和 Xapian::QueryParser，提供全文搜索、
拼写纠错、相似文档推荐、规则引擎搜索等高级搜索功能。
支持中文 CJK 分词、字段特定搜索、布尔操作符 (AND/OR/NOT)。
排序方式：相关性、修改日期、文件大小。
 */

#ifndef ANYTXT_XAPIAN_SEARCHER_H
#define ANYTXT_XAPIAN_SEARCHER_H

#include <xapian.h>
#include "core/document.h"

class SearchRule;
#include <QString>
#include <QVector>
#include <QMap>
#include <QPair>
#include <QMutex>
#include <memory>

class XapianDatabase;

class XapianSearcher {
public:
    explicit XapianSearcher(std::shared_ptr<XapianDatabase> database);
    ~XapianSearcher();

    QPair<QVector<Document>, int> search(
        const QString& query,
        int offset = 0,
        int limit = 100,
        const QMap<QString, QString>& filters = {},
        const QString& sortBy = "relevance",
        bool sortReverse = false,
        const QString& matchType = "and"
    );

    QStringList suggestSpelling(const QString& query);
    QVector<Document> getSimilarDocuments(int64_t docId, int maxResults = 10);
    Document getDocumentById(int64_t docId);
    Document getDocumentByPath(const QString& path);

    // Get all indexed documents (for file browser)
    QVector<Document> getAllDocuments(int offset = 0, int limit = -1);

    // Rule-based advanced search
    QPair<QVector<Document>, int> searchByRule(const class SearchRule& rule,
                                                int offset = 0,
                                                int limit = 100);

private:
    Xapian::Query buildFilterQuery(const QMap<QString, QString>& filters) const;
    Document convertToDocument(const Xapian::Document& xdoc, Xapian::docid docId, double percent) const;

    std::shared_ptr<XapianDatabase> m_database;
    mutable QMutex m_mutex;
};

#endif // ANYTXT_XAPIAN_SEARCHER_H
