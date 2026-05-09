/*
 * xapian_indexer.cpp - AnyTXT Searcher Xapian 索引器实现
 *
 * 功能说明：实现文档的索引添加、删除、清空和优化等操作。
 *          使用 Xapian 的 TermGenerator 进行文本分词和词干提取，
 *          使用 UTF-8 唯一项（unique term）确保同一文件路径
 *          被重复索引时自动覆盖旧文档。
 */

#include "core/xapian_indexer.h"
#include "core/xapian_database.h"
#include "core/exceptions.h"
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <cinttypes>

/**
 * @brief 构造函数
 * @param database 共享的 Xapian 数据库对象指针
 */
XapianIndexer::XapianIndexer(std::shared_ptr<XapianDatabase> database)
    : m_database(std::move(database))
{
}

/**
 * @brief 析构函数
 */
XapianIndexer::~XapianIndexer()
{
}

/**
 * @brief 配置词项生成器
 *
 * 设置词干提取器（如果指定语言不可用则回退到英语），
 * 采用 STEM_SOME 策略（部分词干化），
 * 并启用 CJK（中日韩）N-gram 分词以支持东亚语言。
 *
 * @param termgen 待配置的 TermGenerator 引用
 */
void XapianIndexer::setupTermGenerator(Xapian::TermGenerator& termgen) const
{
    try {
        // 尝试使用配置的语言创建词干提取器
        Xapian::Stem stem(m_stemLanguage.toStdString());
        termgen.set_stemmer(stem);
    } catch (const Xapian::Error& e) {
        // 语言不受支持时回退到英语
        qWarning() << "Stemmer not available for language" << m_stemLanguage
                    << ":" << e.get_description().c_str();
        Xapian::Stem stem("english");
        termgen.set_stemmer(stem);
    }

    // 设置词干化策略：对部分词项进行词干化
    termgen.set_stemming_strategy(Xapian::TermGenerator::STEM_SOME);

    // 启用 CJK N-gram 分词支持中文、日文、韩文
    int flags = Xapian::TermGenerator::FLAG_CJK_NGRAM;
    termgen.set_flags(flags);
}

/**
 * @brief 将 Unix 时间戳编码为值槽字符串
 *
 * 格式为 "yyyyMMddHHmmss"，便于按日期进行范围查询和排序。
 *
 * @param unixTime Unix 时间戳（秒）
 * @return 格式化后的日期字符串
 */
QString XapianIndexer::encodeDateForSlot(qint64 unixTime)
{
    QDateTime dt = QDateTime::fromSecsSinceEpoch(unixTime);
    return dt.toString("yyyyMMddHHmmss");
}

/**
 * @brief 将文件大小编码为值槽字符串
 *
 * 使用 20 位零填充十进制字符串，使字典序排序等价于数值排序。
 *
 * @param size 文件大小（字节）
 * @return 零填充后的字符串
 */
QString XapianIndexer::encodeSizeForSlot(int64_t size)
{
    // 零填充 20 位，确保数值排序正确
    return QString("%1").arg(size, 20, 10, QChar('0'));
}

/**
 * @brief 根据文件路径生成唯一项（unique term）
 *
 * 使用规范化后的绝对文件路径，前缀 "Q" 避免与其他词项冲突。
 * 该唯一项用于 replace_document，确保同一文件重新索引时
 * 自动覆盖旧文档。
 *
 * @param filePath 文件路径
 * @return 唯一项字符串
 */
QString XapianIndexer::uniqueTerm(const QString& filePath)
{
    // 标准化路径后作为唯一项
    QFileInfo fi(filePath);
    QString normalized = fi.absoluteFilePath();
    return "Q" + normalized;
}

/**
 * @brief 将单篇文档添加到索引
 *
 * 流程：
 * 1. 获取可写数据库并创建 Xapian 文档对象
 * 2. 配置词项生成器并索引文本内容
 * 3. 将文档数据（元数据、内容截断）以 JSON 格式存入文档 data 域
 * 4. 写入值槽 0（日期）和值槽 1（大小）以支持排序和过滤
 * 5. 添加布尔词项用于 MIME 类型、扩展名、路径过滤
 * 6. 使用文件路径唯一项替换/更新文档
 * 7. 提交更改并刷新只读数据库
 *
 * @param filePath 文件路径
 * @param metadata 元数据键值对
 * @param content 文本内容
 * @return 文档 ID
 * @throws IndexError 索引操作失败时抛出
 */
int64_t XapianIndexer::addDocument(const QString& filePath,
                                    const QMap<QString, QString>& metadata,
                                    const QString& content)
{
    QMutexLocker locker(&m_mutex);

    // 检查数据库是否可用
    if (!m_database || !m_database->isOpen()) {
        throw IndexError("Database is not open");
    }

    try {
        auto& wdb = m_database->getWritableDatabase();

        // 获取文件信息
        QFileInfo fi(filePath);
        QString fileName = fi.fileName();
        QString fileExt = fi.suffix().toLower();
        QString mimeType = metadata.value("mimeType", "unknown");

        // 创建 Xapian 文档对象
        Xapian::Document doc;

        // 配置词项生成器
        Xapian::TermGenerator termgen;
        setupTermGenerator(termgen);
        termgen.set_document(doc);

        // 索引文本内容
        indexContent(termgen, doc, filePath, mimeType, fileExt, content);

        // 将文档数据以 JSON 格式存储（内容截断至 10K 字符）
        QJsonObject dataObj;
        dataObj["path"] = filePath;
        dataObj["content"] = content.left(10000); // 截断到 10K 字符
        QJsonObject metaObj;
        for (auto it = metadata.constBegin(); it != metadata.constEnd(); ++it) {
            metaObj[it.key()] = it.value();
        }
        dataObj["metadata"] = metaObj;

        QJsonDocument jdoc(dataObj);
        doc.set_data(jdoc.toJson(QJsonDocument::Compact).toStdString());

        // 值槽 0：日期（格式 yyyyMMddHHmmss）
        qint64 mtime = metadata.value("modifiedTime").toLongLong();
        if (mtime <= 0) {
            // 无有效修改时间时使用当前时间
            mtime = QDateTime::currentSecsSinceEpoch();
        }
        doc.add_value(0, encodeDateForSlot(mtime).toStdString());

        // 值槽 1：文件大小（零填充 20 位数字）
        int64_t fsize = metadata.value("fileSize").toLongLong();
        doc.add_value(1, encodeSizeForSlot(fsize).toStdString());

        // 添加布尔词项用于过滤器
        doc.add_boolean_term(("XTYPE" + mimeType.toStdString()));  // MIME 类型过滤
        doc.add_boolean_term(("XEXT" + fileExt.toStdString()));    // 扩展名过滤
        doc.add_boolean_term(("XPATH" + fi.absolutePath().toStdString())); // 路径过滤

        // 使用文件路径作为唯一项，自动替换已有文档
        std::string uterm = uniqueTerm(filePath).toStdString();

        // replace_document 根据唯一项自动处理新增或更新
        Xapian::docid replacedDocId = wdb.replace_document(uterm, doc);

        // 处理拼写建议数据
        if (m_enableSpelling) {
            termgen.set_document(doc);
        }

        // 提交由外部事务控制（见 beginBatch() / commitBatch()）
        // 不在内部自动 commit，由调用方管理批量
        m_docsSinceFlush++;
        int64_t docId = static_cast<int64_t>(replacedDocId);

        return docId;

    } catch (const Xapian::Error& e) {
        QString err = QString("Failed to index document %1: %2")
                        .arg(filePath, e.get_description().c_str());
        qWarning() << err;
        throw IndexError(err);
    }
}

/**
 * @brief 执行文档内容的索引操作
 *
 * 索引策略：
 * - 文件名以 "S" 前缀索引（支持 title: 域搜索）
 * - 文件名小写形式以 "F" 前缀添加为布尔词项（支持 file: 域搜索）
 * - 完整文本内容以无前缀索引（支持全文搜索）
 * - 文件扩展名以 "XEXT" 前缀添加（支持扩展名过滤）
 *
 * @param termgen 词项生成器
 * @param doc Xapian 文档
 * @param filePath 文件路径
 * @param mimeType MIME 类型（当前未使用）
 * @param fileExt 文件扩展名
 * @param content 文本内容
 */
void XapianIndexer::indexContent(Xapian::TermGenerator& termgen,
                                  Xapian::Document& doc,
                                  const QString& filePath,
                                  const QString& mimeType,
                                  const QString& fileExt,
                                  const QString& content) const
{
    Q_UNUSED(mimeType);
    QFileInfo fi(filePath);
    QString title = fi.fileName();

    // 以 "S" 前缀索引标题，支持 title:xxx 域搜索
    termgen.index_text(title.toStdString(), 1, "S");

    // 以 "F" 前缀索引文件名（小写），支持 file:xxx 域搜索
    std::string filename = fi.fileName().toLower().toStdString();
    doc.add_term("F" + filename);

    // 无前缀索引全文内容，支持普通全文搜索
    std::string contentStd = content.toStdString();
    if (!contentStd.empty()) {
        termgen.index_text(contentStd);
    }

    // 添加扩展名布尔词项支持过滤
    doc.add_term(("XEXT" + fileExt.toStdString()));
}

/**
 * @brief 批量添加文档到索引
 *
 * 对路径列表中的每个文件创建最小元数据并调用 addDocument。
 * 如果单个文件索引失败，记录警告并继续处理后续文件。
 *
 * @param paths 文件路径列表
 * @return 成功索引的文档 ID 向量
 */
QVector<int64_t> XapianIndexer::addDocumentsBatch(const QStringList& paths)
{
    QVector<int64_t> docIds;
    docIds.reserve(paths.size());

    for (const auto& path : paths) {
        try {
            // 构建最小元数据集
            QMap<QString, QString> meta;
            QFileInfo fi(path);
            meta["fileSize"] = QString::number(fi.size());
            meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
            meta["mimeType"] = "unknown";
            meta["fileExt"] = fi.suffix().toLower();

            // 执行索引（注：实际内容需提前提取）
            int64_t docId = addDocument(path, meta, "");
            docIds.append(docId);
        } catch (const IndexError& e) {
            qWarning() << "Failed to index" << path << ":" << e.what();
        }
    }

    return docIds;
}

/**
 * @brief 从索引中删除指定路径的文档
 *
 * 通过文件路径的唯一项定位并删除文档。
 *
 * @param filePath 要删除的文件路径
 * @return true 表示删除成功
 */
bool XapianIndexer::deleteDocument(const QString& filePath)
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        qWarning() << "Cannot delete: database not open";
        return false;
    }

    try {
        auto& wdb = m_database->getWritableDatabase();
        // 通过唯一项删除文档
        std::string uterm = uniqueTerm(filePath).toStdString();
        wdb.delete_document(uterm);
        wdb.commit();
        m_database->refresh();
        qDebug() << "Deleted document:" << filePath;
        return true;
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to delete document" << filePath
                    << ":" << e.get_description().c_str();
        return false;
    }
}

/**
 * @brief 清空整个索引
 *
 * 遍历所有 posting，逐条删除所有文档后提交。
 */
void XapianIndexer::clearIndex()
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        qWarning() << "Cannot clear: database not open";
        return;
    }

    try {
        auto& wdb = m_database->getWritableDatabase();
        // 遍历所有文档并删除
        Xapian::PostingIterator it = wdb.postlist_begin("");
        Xapian::PostingIterator end = wdb.postlist_end("");

        int count = 0;
        while (it != end) {
            wdb.delete_document(*it);
            ++it;
            ++count;
        }
        wdb.commit();
        m_database->refresh();
        qDebug() << "Cleared" << count << "documents from index";
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to clear index:" << e.get_description().c_str();
    }
}

void XapianIndexer::beginBatch()
{
    QMutexLocker locker(&m_mutex);
    if (!m_database || !m_database->isOpen()) return;
    auto& wdb = m_database->getWritableDatabase();
    wdb.begin_transaction();
}

void XapianIndexer::commitBatch()
{
    QMutexLocker locker(&m_mutex);
    if (!m_database || !m_database->isOpen()) return;
    try {
        auto& wdb = m_database->getWritableDatabase();
        wdb.commit_transaction();
        m_database->refresh();
        m_docsSinceFlush = 0;
    } catch (const Xapian::Error& e) {
        qWarning() << "Batch commit failed:" << e.get_description().c_str();
    }
}

/**
 * @brief 优化索引
 *
 * 刷写未决的更改并刷新只读数据库。
 *
 * @return true 表示优化成功
 */
bool XapianIndexer::optimize()
{
    QMutexLocker locker(&m_mutex);

    if (!m_database || !m_database->isOpen()) {
        qWarning() << "Cannot optimize: database not open";
        return false;
    }

    try {
        // 刷写未决更改
        auto& wdb = m_database->getWritableDatabase();
        wdb.commit();
        m_database->refresh();

        return true;
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to optimize index:" << e.get_description().c_str();
        return false;
    }
}

void XapianIndexer::flush()
{
    QMutexLocker locker(&m_mutex);
    if (!m_database || !m_database->isOpen()) return;
    if (m_docsSinceFlush == 0) return;
    try {
        auto& wdb = m_database->getWritableDatabase();
        wdb.commit();
        m_database->refresh();
        m_docsSinceFlush = 0;
    } catch (const Xapian::Error& e) {
        qWarning() << "Flush failed:" << e.get_description().c_str();
    }
}

/**
 * @brief 获取索引中的文档总数
 * @return 文档数量（数据库不可用时返回 0）
 */
int XapianIndexer::getDocumentCount() const
{
    if (!m_database || !m_database->isOpen()) return 0;
    return m_database->getDocumentCount();
}

/**
 * @brief 估计索引占用空间
 *
 * 由于无法直接从 Xapian API 获取数据库物理大小，
 * 使用文档数乘以 1024 进行粗略估算。
 *
 * @return 估计的字节数
 */
int64_t XapianIndexer::getIndexSize() const
{
    if (!m_database || !m_database->isOpen()) return 0;

    try {
        // 无法直接从 Xapian 获取数据库大小，使用文档数粗略估算
        return static_cast<int64_t>(m_database->getDocumentCount()) * 1024;
    } catch (...) {
        return 0;
    }
}
