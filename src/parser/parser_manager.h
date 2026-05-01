/*
 * parser_manager.h - 解析器管理器

功能说明：管理所有文档解析器，根据文件扩展名
自动选择合适的解析器进行处理。
 */

#ifndef ANYTXT_PARSER_MANAGER_H
#define ANYTXT_PARSER_MANAGER_H

#include "core/document_processor.h"
#include <QVector>
#include <QMap>
#include <memory>

class ParserManager {
public:
    ParserManager();
    ~ParserManager();

    void registerProcessor(std::shared_ptr<DocumentProcessor> processor);
    DocumentProcessor* getProcessor(const QString& filePath) const;
    QStringList supportedExtensions() const;

    DocumentProcessor::Result processDocument(const QString& filePath);

    // Tesseract OCR helper for images
    static QString ocrImage(const QString& imagePath);

private:
    QVector<std::shared_ptr<DocumentProcessor>> m_processors;
    QMap<QString, std::shared_ptr<DocumentProcessor>> m_extensionMap;
};

#endif // ANYTXT_PARSER_MANAGER_H
