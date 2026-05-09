/*
 * xapian_searcher.cpp - AnyTXT Searcher Xapian 搜索器实现
 *
 * 功能说明：封装 Xapian::Enquire 和 Xapian::QueryParser 进行文档搜索，
 * 支持全文搜索、拼写纠错、相似文档推荐、高级规则搜索、结果排序及过滤。
 * 支持多分片搜索：通过 Xapian::Database(shard0, shard1, ...) 组合多个
 * 分片数据库，搜索时自动合并去重。
 */

#include "core/xapian_searcher.h"
#include "core/xapian_database.h"
#include "core/rule_engine.h"
#include "core/exceptions.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <cmath>

// =====================================================================
// 构造 / 析构
// =====================================================================

XapianSearcher::XapianSearcher(
    const QVector<QString>& shardPaths,
    const QVector<std::shared_ptr<XapianDatabase>>& databases)
    : m_shardPaths(shardPaths)
    , m_databases(databases)
{
}

XapianSearcher::~XapianSearcher()
{
}

// =====================================================================
// 打开组合数据库
// =====================================================================

// buildCompositeDb used below in all search methods

// Helper to build composite database from N shards
static Xapian::Database buildCompositeDb(const QVector<QString>& paths)
{
    if (paths.isEmpty()) {
        throw SearchException("No database shards configured");
    }
    Xapian::Database db(paths[0].toStdString());
    for (int i = 1; i < paths.size(); ++i) {
        db.add_database(Xapian::Database(paths[i].toStdString()));
    }
    return db;
}

// =====================================================================
// 过滤器查询构建
// =====================================================================

Xapian::Query XapianSearcher::buildFilterQuery(
    const QMap<QString, QString>& filters) const
{
    QVector<Xapian::Query> clauses;
    clauses.reserve(8);

    for (auto it = filters.constBegin(); it != filters.constEnd(); ++it) {
        const QString& key   = it.key();
        const QString& value = it.value();
        if (value.isEmpty()) continue;

        if (key == "mimeType") {
            clauses.append(Xapian::Query("XTYPE" + value.toStdString()));
        } else if (key == "fileExt") {
            clauses.append(Xapian::Query("XEXT" + value.toStdString()));
        } else if (key == "filePath") {
            clauses.append(Xapian::Query("XPATH" + value.toStdString()));
        } else if (key == "title") {
            Xapian::QueryParser parser;
            parser.add_prefix("title", "S");
            clauses.append(parser.parse_query(value.toStdString(),
                Xapian::QueryParser::FLAG_DEFAULT));
        } else if (key == "content") {
            Xapian::QueryParser parser;
            clauses.append(parser.parse_query(value.toStdString(),
                Xapian::QueryParser::FLAG_DEFAULT));
        } else if (key == "sizeMin") {
            bool ok = false;
            int64_t sz = value.toLongLong(&ok);
            if (ok) {
                std::string encoded = Xapian::sortable_serialise(
                    static_cast<double>(sz));
                clauses.append(Xapian::Query(Xapian::Query::OP_VALUE_GE, 1, encoded));
            }
        } else if (key == "sizeMax") {
            bool ok = false;
            int64_t sz = value.toLongLong(&ok);
            if (ok) {
                std::string encoded = Xapian::sortable_serialise(
                    static_cast<double>(sz));
                clauses.append(Xapian::Query(Xapian::Query::OP_VALUE_LE, 1, encoded));
            }
        } else if (key == "dateMin") {
            clauses.append(Xapian::Query(
                Xapian::Query::OP_VALUE_GE, 0, value.toStdString()));
        } else if (key == "dateMax") {
            clauses.append(Xapian::Query(
                Xapian::Query::OP_VALUE_LE, 0, value.toStdString()));
        }
    }

    if (clauses.isEmpty()) {
        return Xapian::Query(std::string());
    }
    if (clauses.size() == 1) {
        return clauses.first();
    }
    return Xapian::Query(Xapian::Query::OP_AND, clauses.begin(), clauses.end());
}

// =====================================================================
// 文档转换
// =====================================================================

Document XapianSearcher::convertToDocument(
    const Xapian::Document& xdoc, Xapian::docid docId, double percent) const
{
    Document doc;
    doc.docId     = static_cast<int64_t>(docId);
    doc.percent   = static_cast<int>(std::round(percent));
    doc.relevance = percent;
    doc.rank      = 0;

    std::string rawData = xdoc.get_data();
    QJsonDocument jdoc = QJsonDocument::fromJson(
        QByteArray::fromStdString(rawData));
    if (jdoc.isObject()) {
        QJsonObject obj = jdoc.object();
        doc.filePath = obj.value("path").toString();
        doc.content  = obj.value("content").toString();

        QJsonObject meta = obj.value("metadata").toObject();
        doc.fileSize     = meta.value("fileSize").toString().toLongLong();
        doc.modifiedTime = meta.value("modifiedTime").toString().toLongLong();
        doc.mimeType     = meta.value("mimeType").toString();
        doc.fileExt      = QFileInfo(doc.filePath).suffix().toLower();
        doc.fileName     = QFileInfo(doc.filePath).fileName();
        doc.title        = doc.fileName;

        for (auto it = meta.constBegin(); it != meta.constEnd(); ++it) {
            doc.metadata[it.key()] = it.value().toString();
        }
    }
    return doc;
}

// =====================================================================
// 全文搜索（核心方法）
// =====================================================================

QPair<QVector<Document>, int> XapianSearcher::search(
    const QString& query,
    int offset,
    int limit,
    const QMap<QString, QString>& filters,
    const QString& sortBy,
    bool sortReverse,
    const QString& matchType)
{
    QMutexLocker locker(&m_mutex);

    if (m_shardPaths.isEmpty()) {
        throw SearchException("No database shards configured");
    }

    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);

        // --- 1. 解析查询 ---
        Xapian::QueryParser parser;
        parser.set_database(db);
        parser.set_stemmer(Xapian::Stem("english"));
        parser.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);

        if (matchType == "or") {
            parser.set_default_op(Xapian::Query::OP_OR);
        } else {
            parser.set_default_op(Xapian::Query::OP_AND);
        }

        parser.add_prefix("title", "S");
        parser.add_prefix("file",  "F");
        parser.add_prefix("filename", "F");
        parser.add_prefix("type",  "XTYPE");
        parser.add_prefix("ext",   "XEXT");
        parser.add_prefix("mime",  "XMIME");

        unsigned flags = Xapian::QueryParser::FLAG_DEFAULT
                       | Xapian::QueryParser::FLAG_BOOLEAN
                       | Xapian::QueryParser::FLAG_PHRASE
                       | Xapian::QueryParser::FLAG_LOVEHATE
                       | Xapian::QueryParser::FLAG_WILDCARD
                       | Xapian::QueryParser::FLAG_CJK_NGRAM;

        Xapian::Query parsed;
        QString qtrim = query.trimmed();
        if (qtrim.isEmpty()) {
            parsed = Xapian::Query(std::string());
        } else {
            if (matchType == "phrase") {
                qtrim = "\"" + qtrim + "\"";
            }
            qtrim = qtrim.simplified();
            qtrim.replace(QRegularExpression("\\s*&\\s*"), " AND ");
            qtrim.replace(QRegularExpression("\\s*\\|\\s*"), " OR ");
            qtrim.replace(QRegularExpression("\\s+-\\s+"), " NOT ");
            qtrim = qtrim.simplified();

            parsed = parser.parse_query(qtrim.toStdString(), flags);
        }

        // --- 2. 构建过滤器 ---
        Xapian::Query filterQ = buildFilterQuery(filters);

        // --- 3. 合并查询与过滤器 ---
        Xapian::Query finalQuery;
        if (filterQ.empty()) {
            finalQuery = parsed;
        } else {
            finalQuery = Xapian::Query(Xapian::Query::OP_FILTER, parsed, filterQ);
        }

        // --- 4. 执行搜索 ---
        Xapian::Enquire enquire(db);
        enquire.set_query(finalQuery);

        if (sortBy == "date" || sortBy == "modified") {
            enquire.set_sort_by_value_then_relevance(0, !sortReverse);
        } else if (sortBy == "size") {
            enquire.set_sort_by_value_then_relevance(1, !sortReverse);
        } else {
            enquire.set_sort_by_relevance();
        }

        // --- 5. 获取结果 ---
        Xapian::MSet mset = enquire.get_mset(offset, limit);

        QVector<Document> results;
        results.reserve(static_cast<int>(mset.size()));

        int rank = offset;
        for (auto it = mset.begin(); it != mset.end(); ++it) {
            Document doc = convertToDocument(it.get_document(),
                                              *it,
                                              it.get_percent());
            doc.rank = rank++;
            results.append(std::move(doc));
        }

        int total = static_cast<int>(mset.get_matches_estimated());
        return {results, total};

    } catch (const Xapian::Error& e) {
        QString err = QString("Search failed: %1").arg(e.get_description().c_str());
        qWarning() << err;
        throw SearchException(err);
    }
}

// =====================================================================
// 刷新组合数据库
// =====================================================================

void XapianSearcher::refresh()
{
    QMutexLocker locker(&m_mutex);
    for (auto& db : m_databases) {
        if (db && db->isOpen()) db->refresh();
    }
}

// =====================================================================
// 其他搜索方法
// =====================================================================

QStringList XapianSearcher::suggestSpelling(const QString& query)
{
    QMutexLocker locker(&m_mutex);
    if (m_shardPaths.isEmpty()) return {};

    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);
        QStringList suggestions;
        QStringList terms = query.split(' ', Qt::SkipEmptyParts);
        for (const QString& term : terms) {
            std::string s = db.get_spelling_suggestion(term.toStdString(), 2);
            if (!s.empty()) {
                suggestions.append(QString::fromStdString(s));
            }
        }
        return suggestions;
    } catch (const Xapian::Error& e) {
        qWarning() << "Spelling suggestion failed:" << e.get_description().c_str();
        return {};
    }
}

QVector<Document> XapianSearcher::getSimilarDocuments(
    int64_t docId, int maxResults)
{
    QMutexLocker locker(&m_mutex);
    if (m_shardPaths.isEmpty()) throw SearchException("No database shards");

    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);
        Xapian::Document sourceDoc = db.get_document(
            static_cast<Xapian::docid>(docId));

        QVector<Xapian::Query> termQueries;
        termQueries.reserve(64);

        Xapian::TermIterator termIt = sourceDoc.termlist_begin();
        Xapian::TermIterator termEnd = sourceDoc.termlist_end();

        struct TermInfo {
            std::string term;
            Xapian::termcount wdf;
        };
        QVector<TermInfo> terms;
        for (; termIt != termEnd; ++termIt) {
            std::string tname = *termIt;
            if (!tname.empty() && !std::isupper(static_cast<unsigned char>(tname[0]))) {
                terms.push_back({tname, termIt.get_wdf()});
            }
        }

        if (terms.isEmpty()) return {};

        std::sort(terms.begin(), terms.end(),
            [](const TermInfo& a, const TermInfo& b) {
                return a.wdf > b.wdf;
            });

        int maxTerms = qMin(static_cast<int>(terms.size()), 64);
        for (int i = 0; i < maxTerms; ++i) {
            termQueries.append(Xapian::Query(terms[i].term));
        }

        Xapian::Query similarQuery(
            Xapian::Query::OP_OR, termQueries.begin(), termQueries.end());
        Xapian::Enquire enquire(db);
        enquire.set_query(similarQuery);

        Xapian::MSet mset = enquire.get_mset(0, maxResults + 1); // +1 to skip source

        QVector<Document> results;
        for (auto it = mset.begin(); it != mset.end(); ++it) {
            if (static_cast<int64_t>(*it) == docId) continue; // skip source
            Document doc = convertToDocument(it.get_document(), *it, it.get_percent());
            doc.rank = results.size();
            results.append(std::move(doc));
            if (results.size() >= maxResults) break;
        }
        return results;
    } catch (const Xapian::DocNotFoundError&) {
        return {};
    } catch (const Xapian::Error& e) {
        qWarning() << "getSimilarDocuments failed:" << e.get_description().c_str();
        return {};
    }
}

Document XapianSearcher::getDocumentById(int64_t docId)
{
    QMutexLocker locker(&m_mutex);
    if (m_shardPaths.isEmpty()) return {};
    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);
        Xapian::Document xdoc = db.get_document(
            static_cast<Xapian::docid>(docId));
        return convertToDocument(xdoc, static_cast<Xapian::docid>(docId), 100.0);
    } catch (const Xapian::DocNotFoundError&) {
        return {};
    } catch (const Xapian::Error& e) {
        qWarning() << "getDocumentById failed:" << e.get_description().c_str();
        throw SearchException(QString::fromStdString(e.get_description()));
    }
}

Document XapianSearcher::getDocumentByPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    if (m_shardPaths.isEmpty()) return {};
    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);
        QFileInfo fi(path);
        std::string uterm = ("Q" + fi.absoluteFilePath()).toStdString();
        auto it = db.postlist_begin(uterm);
        auto end = db.postlist_end(uterm);
        if (it == end) {
            Document empty;
            empty.docId = -1;
            return empty;
        }
        Xapian::docid docId = *it;
        Xapian::Document xdoc = db.get_document(docId);
        return convertToDocument(xdoc, docId, 100.0);
    } catch (const Xapian::DocNotFoundError&) {
        Document empty;
        empty.docId = -1;
        return empty;
    } catch (const Xapian::Error& e) {
        qWarning() << "getDocumentByPath failed:" << e.get_description().c_str();
        throw SearchException(QString::fromStdString(e.get_description()));
    }
}

QVector<Document> XapianSearcher::getAllDocuments(int offset, int limit)
{
    QMutexLocker locker(&m_mutex);
    if (m_shardPaths.isEmpty()) return {};

    try {
        Xapian::Database db = buildCompositeDb(m_shardPaths);
        int total = static_cast<int>(db.get_doccount());
        if (total == 0) return {};

        if (offset < 0) offset = 0;
        if (limit < 0) limit = total;
        limit = qMin(limit, total - offset);

        Xapian::Enquire enquire(db);
        enquire.set_query(Xapian::Query(std::string()));
        Xapian::MSet mset = enquire.get_mset(offset, limit);

        QVector<Document> results;
        results.reserve(static_cast<int>(mset.size()));
        for (auto it = mset.begin(); it != mset.end(); ++it) {
            results.append(convertToDocument(it.get_document(), *it, 100.0));
        }
        return results;
    } catch (const Xapian::Error& e) {
        qWarning() << "getAllDocuments failed:" << e.get_description().c_str();
        return {};
    }
}

QPair<QVector<Document>, int> XapianSearcher::searchByRule(
    const SearchRule& rule, int offset, int limit)
{
    RuleEngine engine;
    QString queryStr = engine.parseRule(rule);
    QMap<QString, QString> filters;

    for (const auto& cond : rule.conditions()) {
        switch (cond.field) {
            case RuleField::FILE_TYPE:
                filters["mimeType"] = cond.value;
                break;
            case RuleField::FILE_EXTENSION:
                filters["fileExt"] = cond.value;
                break;
            case RuleField::FILE_SIZE:
                if (cond.op == RuleOp::GREATER_THAN) {
                    filters["sizeMin"] = cond.value;
                } else if (cond.op == RuleOp::LESS_THAN) {
                    filters["sizeMax"] = cond.value;
                } else if (cond.op == RuleOp::BETWEEN && cond.valueList.size() >= 2) {
                    filters["sizeMin"] = cond.valueList[0];
                    filters["sizeMax"] = cond.valueList[1];
                }
                break;
            case RuleField::MODIFIED_DATE:
            case RuleField::CREATED_DATE:
                if (cond.op == RuleOp::GREATER_THAN) {
                    filters["dateMin"] = cond.value;
                } else if (cond.op == RuleOp::LESS_THAN) {
                    filters["dateMax"] = cond.value;
                } else if (cond.op == RuleOp::BETWEEN && cond.valueList.size() >= 2) {
                    filters["dateMin"] = cond.valueList[0];
                    filters["dateMax"] = cond.valueList[1];
                }
                break;
            default:
                break;
        }
    }
    return search(queryStr, offset, limit, filters, "relevance", false);
}
