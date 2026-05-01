/*
 * xapian_searcher.cpp - AnyTXT Searcher Xapian 搜索器实现
 *
 * 功能说明：封装 Xapian::Enquire 和 Xapian::QueryParser 进行文档搜索，
 * 支持全文搜索、拼写纠错、相似文档推荐、高级规则搜索、结果排序及过滤。
 * 使用 Xapian::Query(std::string()) 构造空查询以表示不限制匹配条件。
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

XapianSearcher::XapianSearcher(std::shared_ptr<XapianDatabase> database)
    : m_database(std::move(database))
{
}

XapianSearcher::~XapianSearcher()
{
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
            // 布尔词项 XTYPE{value}
            clauses.append(Xapian::Query("XTYPE" + value.toStdString()));
        } else if (key == "fileExt") {
            // 布尔词项 XEXT{value}
            clauses.append(Xapian::Query("XEXT" + value.toStdString()));
        } else if (key == "filePath") {
            // 布尔词项 XPATH{value}
            clauses.append(Xapian::Query("XPATH" + value.toStdString()));
        } else if (key == "title") {
            // 标题域搜索（前缀 S）
            Xapian::QueryParser parser;
            parser.add_prefix("title", "S");
            Xapian::Query q = parser.parse_query(value.toStdString(),
                Xapian::QueryParser::FLAG_DEFAULT);
            clauses.append(q);
        } else if (key == "content") {
            // 全文搜索（无前缀）
            Xapian::QueryParser parser;
            Xapian::Query q = parser.parse_query(value.toStdString(),
                Xapian::QueryParser::FLAG_DEFAULT);
            clauses.append(q);
        } else if (key == "sizeMin") {
            // 值槽 1（文件大小）：>= value
            bool ok = false;
            int64_t sz = value.toLongLong(&ok);
            if (ok) {
                std::string encoded = Xapian::sortable_serialise(
                    static_cast<double>(sz));
                clauses.append(Xapian::Query(Xapian::Query::OP_VALUE_GE, 1, encoded));
            }
        } else if (key == "sizeMax") {
            // 值槽 1（文件大小）：<= value
            bool ok = false;
            int64_t sz = value.toLongLong(&ok);
            if (ok) {
                std::string encoded = Xapian::sortable_serialise(
                    static_cast<double>(sz));
                clauses.append(Xapian::Query(Xapian::Query::OP_VALUE_LE, 1, encoded));
            }
        } else if (key == "dateMin") {
            // 值槽 0（修改日期）：>= value (yyyyMMddHHmmss)
            clauses.append(Xapian::Query(
                Xapian::Query::OP_VALUE_GE, 0, value.toStdString()));
        } else if (key == "dateMax") {
            // 值槽 0（修改日期）：<= value
            clauses.append(Xapian::Query(
                Xapian::Query::OP_VALUE_LE, 0, value.toStdString()));
        }
    }

    if (clauses.isEmpty()) {
        // 无过滤条件时返回 MatchAll 等价查询
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
    doc.rank      = 0; // 由调用者在排序后填入

    // 解析文档 data 字段中的 JSON
    std::string rawData = xdoc.get_data();
    QJsonDocument jdoc = QJsonDocument::fromJson(
        QByteArray::fromStdString(rawData));
    if (jdoc.isObject()) {
        QJsonObject obj = jdoc.object();
        doc.filePath = obj.value("path").toString();
        doc.content  = obj.value("content").toString();

        // 从 metadata 子对象中提取元数据
        QJsonObject meta = obj.value("metadata").toObject();
        doc.fileSize     = meta.value("fileSize").toString().toLongLong();
        doc.modifiedTime = meta.value("modifiedTime").toString().toLongLong();
        doc.mimeType     = meta.value("mimeType").toString();
        doc.fileExt      = QFileInfo(doc.filePath).suffix().toLower();
        doc.fileName     = QFileInfo(doc.filePath).fileName();
        doc.title        = doc.fileName;

        // 收集剩余元数据
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

    if (!m_database || !m_database->isOpen()) {
        throw SearchException("Database is not open");
    }

    Xapian::Database& db = m_database->getDatabase();

    try {
        // --- 1. 解析查询 ---
        Xapian::QueryParser parser;
        parser.set_database(db);
        parser.set_stemmer(Xapian::Stem("english"));
        parser.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);

        // Set default operator based on match type
        if (matchType == "or") {
            parser.set_default_op(Xapian::Query::OP_OR);
        } else if (matchType == "phrase") {
            // Phrase mode: wrap entire query in quotes
            // This is handled below
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
            // Phrase mode: wrap entire query in quotes for exact phrase matching
            if (matchType == "phrase") {
                qtrim = "\"" + qtrim + "\"";
            }

            // Normalize operators: &→AND, |→OR, -→NOT (flexible spacing, multiple spaces collapsed)
            // First collapse multiple spaces to single
            qtrim = qtrim.simplified();
            // & and | with optional surrounding spaces
            qtrim.replace(QRegularExpression("\\s*&\\s*"), " AND ");
            qtrim.replace(QRegularExpression("\\s*\\|\\s*"), " OR ");
            // - between words (with optional space before, at least one space after)
            qtrim.replace(QRegularExpression("\\s+-\\s+"), " NOT ");
            // Recollapse multiple spaces from replacements
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

        // 排序
        if (sortBy == "date" || sortBy == "modified") {
            // 值槽 0：日期（yyyyMMddHHmmss），默认倒序（最新的在前）
            enquire.set_sort_by_value_then_relevance(0, !sortReverse);
        } else if (sortBy == "size") {
            // 值槽 1：文件大小，默认倒序（最大的在前）
            enquire.set_sort_by_value_then_relevance(1, !sortReverse);
        } else {
            // 默认按相关性排序
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
// 拼写建议
// =====================================================================

QStringList XapianSearcher::suggestSpelling(const QString& query)
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        return {};
    }

    try {
        Xapian::Database& db = m_database->getDatabase();
        QStringList suggestions;

        // 按空格拆分查询词，对每个词单独生成拼写建议
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

// =====================================================================
// 相似文档推荐
// =====================================================================

QVector<Document> XapianSearcher::getSimilarDocuments(
    int64_t docId, int maxResults)
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        throw SearchException("Database is not open");
    }

    Xapian::Database& db = m_database->getDatabase();

    try {
        // 获取源文档
        Xapian::Document sourceDoc = db.get_document(
            static_cast<Xapian::docid>(docId));

        // 使用源文档的重要词项构造查询
        Xapian::QueryParser parser;
        parser.set_database(db);

        // 收集文档中出现频率最高的词项
        QVector<Xapian::Query> termQueries;
        termQueries.reserve(64);

        Xapian::TermIterator termIt = sourceDoc.termlist_begin();
        Xapian::TermIterator termEnd = sourceDoc.termlist_end();

        // 收集所有词项，排除内部布尔词项（以大写字母开头）
        // 同时收集词频信息用于加权
        struct TermInfo {
            std::string term;
            Xapian::termcount wdf; // within document frequency
        };
        QVector<TermInfo> terms;
        terms.reserve(128);

        for (; termIt != termEnd; ++termIt) {
            std::string tname = *termIt;
            // 跳过布尔词项（以大写字母开头或含 X 前缀）
            if (!tname.empty() && std::isupper(static_cast<unsigned char>(tname[0]))) {
                continue;
            }
            Xapian::termcount wdf = termIt.get_wdf();
            terms.append({tname, wdf});
        }

        if (terms.isEmpty()) {
            return {};
        }

        // 按词频排序，取最重要的前 64 个词项
        std::sort(terms.begin(), terms.end(),
            [](const TermInfo& a, const TermInfo& b) {
                return a.wdf > b.wdf;
            });

        int take = std::min(64, static_cast<int>(terms.size()));
        for (int i = 0; i < take; ++i) {
            double weight = static_cast<double>(terms[i].wdf) / 10.0 + 1.0;
            termQueries.append(Xapian::Query(terms[i].term, weight));
        }

        if (termQueries.isEmpty()) {
            return {};
        }

        Xapian::Query simQuery(
            Xapian::Query::OP_OR, termQueries.begin(), termQueries.end());

        // 执行搜索（排除源文档自身）
        Xapian::Enquire enquire(db);
        enquire.set_query(simQuery);

        // 请求多取一个（可能包含源文档，需要跳过）
        int fetchCount = maxResults + 1;
        Xapian::MSet mset = enquire.get_mset(0, fetchCount);

        QVector<Document> results;
        results.reserve(std::min(maxResults, static_cast<int>(mset.size())));

        for (auto it = mset.begin(); it != mset.end(); ++it) {
            // 跳过源文档自身
            if (static_cast<int64_t>(*it) == docId) {
                continue;
            }

            Document doc = convertToDocument(it.get_document(),
                                              *it,
                                              it.get_percent());
            doc.rank = results.size();
            results.append(std::move(doc));

            if (results.size() >= maxResults) {
                break;
            }
        }

        return results;

    } catch (const Xapian::Error& e) {
        QString err = QString("getSimilarDocuments failed: %1")
                        .arg(e.get_description().c_str());
        qWarning() << err;
        throw SearchException(err);
    }
}

// =====================================================================
// 按文档 ID 获取
// =====================================================================

Document XapianSearcher::getDocumentById(int64_t docId)
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        throw SearchException("Database is not open");
    }

    Xapian::Database& db = m_database->getDatabase();

    try {
        Xapian::Document xdoc = db.get_document(
            static_cast<Xapian::docid>(docId));
        return convertToDocument(xdoc, static_cast<Xapian::docid>(docId), 100.0);
    } catch (const Xapian::DocNotFoundError&) {
        Document empty;
        empty.docId = -1;
        return empty;
    } catch (const Xapian::Error& e) {
        QString err = QString("getDocumentById failed: %1")
                        .arg(e.get_description().c_str());
        qWarning() << err;
        throw SearchException(err);
    }
}

// =====================================================================
// 按路径获取
// =====================================================================

Document XapianSearcher::getDocumentByPath(const QString& path)
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        throw SearchException("Database is not open");
    }

    Xapian::Database& db = m_database->getDatabase();

    try {
        // 使用与索引器相同的唯一项格式："Q" + 绝对路径
        QFileInfo fi(path);
        std::string uterm = ("Q" + fi.absoluteFilePath()).toStdString();

        // 通过 postlist 查找文档
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
        QString err = QString("getDocumentByPath failed: %1")
                        .arg(e.get_description().c_str());
        qWarning() << err;
        throw SearchException(err);
    }
}

// =====================================================================
// 获取所有索引文档（文件浏览器用）
// =====================================================================

QVector<Document> XapianSearcher::getAllDocuments(int offset, int limit)
{
    QMutexLocker locker(&m_mutex);
    if (!m_database || !m_database->isOpen()) return {};

    try {
        Xapian::Database& db = m_database->getDatabase();
        int total = static_cast<int>(db.get_doccount());
        if (total == 0) return {};

        if (offset < 0) offset = 0;
        if (limit < 0) limit = total;
        limit = qMin(limit, total - offset);

        // Use MatchAll to enumerate documents
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

// =====================================================================
// 规则引擎搜索
// =====================================================================

QPair<QVector<Document>, int> XapianSearcher::searchByRule(
    const SearchRule& rule, int offset, int limit)
{
    RuleEngine engine;
    QString queryStr = engine.parseRule(rule);

    // 从规则中提取过滤器条件（文件类型、扩展名、大小、日期等）
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
