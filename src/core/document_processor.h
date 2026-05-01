/*
 * document_processor.h - AnyTXT Searcher 文档处理器抽象基类
 *
 * 功能说明：定义文档处理器的纯虚接口，用于从不同类型的文件中
 *          提取文本内容和元数据。具体的文本提取逻辑（如 PDF 解析、
 *          DOCX 解析等）由继承此接口的子类实现。
 */

#ifndef ANYTXT_DOCUMENT_PROCESSOR_H
#define ANYTXT_DOCUMENT_PROCESSOR_H

#include <QString>
#include <QMap>
#include <QVector>
#include <memory>

/**
 * @brief 文档处理器抽象基类
 *
 * 定义了一组纯虚方法，子类需要实现对特定文件格式的文本提取、
 * 元数据提取、格式支持判断等功能。这是"策略模式"中的抽象策略角色。
 */
class DocumentProcessor {
public:
    DocumentProcessor() = default;
    virtual ~DocumentProcessor() = default;

    /**
     * @brief 文档处理结果结构体
     *
     * 包含提取出的纯文本、元数据、处理状态和错误信息。
     */
    struct Result {
        QString text;                          ///< 提取出的纯文本内容
        QMap<QString, QString> metadata;       ///< 提取出的文件元数据
        bool success = false;                  ///< 处理是否成功
        QString errorMessage;                  ///< 处理失败时的错误信息
    };

    /**
     * @brief 获取处理器名称
     * @return 处理器名称字符串（如 "PDF Processor"）
     */
    virtual QString name() const = 0;

    /**
     * @brief 从文件中提取纯文本
     * @param filePath 要处理的文件路径
     * @return 包含提取文本和处理状态的 Result 对象
     */
    virtual Result extractText(const QString& filePath) = 0;

    /**
     * @brief 从文件中提取元数据
     * @param filePath 要处理的文件路径
     * @return 键值对形式的元数据映射
     */
    virtual QMap<QString, QString> extractMetadata(const QString& filePath) = 0;

    /**
     * @brief 判断此处理器能否处理指定文件
     * @param filePath 文件路径（用于按扩展名和 MIME 类型判断）
     * @return true 表示可以处理该文件
     */
    virtual bool canProcess(const QString& filePath) const = 0;

    /**
     * @brief 获取此处理器支持的文件扩展名列表
     * @return 扩展名列表（不含前导点号，如 ["pdf", "docx"]）
     */
    virtual QStringList supportedExtensions() const = 0;

    /**
     * @brief 获取此处理器支持的 MIME 类型列表
     * @return MIME 类型列表（如 ["application/pdf"]）
     */
    virtual QStringList supportedMimeTypes() const = 0;
};

#endif // ANYTXT_DOCUMENT_PROCESSOR_H
