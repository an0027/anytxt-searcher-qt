/*
 * xapian_indexer.h - AnyTXT Searcher Xapian 索引器类
 *
 * 功能说明：提供文档添加到 Xapian 索引、删除文档、清空索引、
 *          索引优化等功能。支持词干提取、拼写检查、批量索引
 *          以及用于过滤和排序的值槽编码。
 */

#ifndef ANYTXT_XAPIAN_INDEXER_H
#define ANYTXT_XAPIAN_INDEXER_H

#include <xapian.h>
#include <QString>
#include <QVector>
#include <QMap>
#include <QMutex>
#include <memory>

class XapianDatabase;

/**
 * @brief Xapian 索引器类
 *
 * 负责将文档内容转换为 Xapian 倒排索引，支持全文检索。
 * 管理词干提取（stemming）、拼写检查（spelling）、
 * 批量提交（batch）等索引参数。通过共享指针持有数据库实例。
 */
class XapianIndexer {
public:
    /**
     * @brief 构造函数
     * @param database 共享的 Xapian 数据库对象指针
     */
    explicit XapianIndexer(std::shared_ptr<XapianDatabase> database);
    ~XapianIndexer();

    /**
     * @brief 将单篇文档添加到索引
     * @param filePath 文件路径（用作文档唯一标识）
     * @param metadata 文档元数据（MIME 类型、大小、修改时间等）
     * @param content 文档文本内容
     * @return 文档在数据库中的 ID
     * @throws IndexError 索引操作失败时抛出
     */
    int64_t addDocument(const QString& filePath,
                        const QMap<QString, QString>& metadata,
                        const QString& content);

    /**
     * @brief 开始批量事务
     *
     * 开始一个 Xapian 事务，后续 addDocument 调用暂不刷盘，
     * 直到 commitBatch() 或 commit_transaction() 调用。
     * 事务内禁止自动 commit，确保批量写入原子性。
     */
    void beginBatch();

    /**
     * @brief 提交批量事务
     *
     * 提交事务并刷盘。事务提交后新索引数据才可被搜索。
     */
    void commitBatch();

    /**
     * @brief 批量添加文档到索引
     * @param paths 文件路径列表
     * @return 成功索引的文档 ID 向量
     */
    QVector<int64_t> addDocumentsBatch(const QStringList& paths);

    /**
     * @brief 从索引中删除指定路径的文档
     * @param filePath 要删除的文件路径
     * @return true 表示删除成功
     */
    bool deleteDocument(const QString& filePath);

    /**
     * @brief 清空整个索引
     */
    void clearIndex();

    /**
     * @brief 优化索引（提交未决更改并刷新只读数据库）
     * @return true 表示优化成功
     */
    bool optimize();

    /**
     * @brief 批量提交待写入的数据并刷新只读数据库
     */
    void flush();

    /**
     * @brief 获取索引中的文档总数
     * @return 文档数量
     */
    int getDocumentCount() const;

    /**
     * @brief 估计索引占用空间大小
     * @return 估计的字节数
     */
    int64_t getIndexSize() const;

public:
    /**
     * @brief 将时间戳编码为值槽字符串
     *
     * 格式为 "yyyyMMddHHmmss"，用于值槽 0 的日期索引和排序。
     *
     * @param unixTime Unix 时间戳（秒）
     * @return 编码后的字符串
     */
    static QString encodeDateForSlot(qint64 unixTime);

    /**
     * @brief 将文件大小编码为值槽字符串
     *
     * 使用 20 位数字零填充，确保按字典序排序等价于按数值排序。
     * 用于值槽 1 的文件大小过滤和排序。
     *
     * @param size 文件大小（字节）
     * @return 编码后的字符串
     */
    static QString encodeSizeForSlot(int64_t size);

    /**
     * @brief 获取当前词干语言
     * @return 词干语言名称
     */
    QString stemLanguage() const { return m_stemLanguage; }

    /**
     * @brief 设置词干提取语言
     * @param lang 语言名称（如 "english", "chinese"）
     */
    void setStemLanguage(const QString& lang) { m_stemLanguage = lang; }

    /**
     * @brief 检查拼写检查是否启用
     * @return true 表示启用
     */
    bool enableSpelling() const { return m_enableSpelling; }

    /**
     * @brief 启用/禁用拼写检查
     * @param val true 启用，false 禁用
     */
    void setEnableSpelling(bool val) { m_enableSpelling = val; }

    /**
     * @brief 获取批量索引的单批文档数
     * @return 批处理大小
     */
    int batchSize() const { return m_batchSize; }

    /**
     * @brief 设置批处理大小
     * @param size 每批文档数
     */
    void setBatchSize(int size) { m_batchSize = size; }

private:
    /**
     * @brief 根据文件路径生成 Xapian 唯一项（用于替换或删除文档）
     * @param filePath 文件路径
     * @return 以 "Q" 为前缀的唯一项字符串
     */
    static QString uniqueTerm(const QString& filePath);

    /**
     * @brief 配置词项生成器的词干提取和 CJK 分词参数
     * @param termgen 待配置的 TermGenerator 引用
     */
    void setupTermGenerator(Xapian::TermGenerator& termgen) const;

    /**
     * @brief 执行文档内容的索引（含标题、文件名、内容的词项生成）
     * @param termgen 词项生成器
     * @param doc 待填充的 Xapian 文档
     * @param filePath 文件路径
     * @param mimeType MIME 类型
     * @param fileExt 文件扩展名
     * @param content 文本内容
     */
    void indexContent(Xapian::TermGenerator& termgen,
                      Xapian::Document& doc,
                      const QString& filePath,
                      const QString& mimeType,
                      const QString& fileExt,
                      const QString& content) const;

    std::shared_ptr<XapianDatabase> m_database; ///< 共享的 Xapian 数据库对象
    QString m_stemLanguage = "english";         ///< 词干提取语言
    bool m_enableSpelling = false;              ///< 是否启用拼写检查
    int m_batchSize = 2000;                     ///< 批量索引批大小
    int m_docsSinceFlush = 0;                   ///< 自上次提交以来的文档数
    mutable QMutex m_mutex;                     ///< 线程安全互斥锁
};

#endif // ANYTXT_XAPIAN_INDEXER_H
