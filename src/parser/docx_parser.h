/*
 * docx_parser.h - DOCX 文件解析器

功能说明：解析 .docx 格式的 Word 文档，
提取文本内容和元数据，基于 ZIP 和 XML 解析。
条件编译：仅在 HAS_LIBZIP 定义时启用完整实现。
 */

#ifndef ANYTXT_DOCX_PARSER_H
#define ANYTXT_DOCX_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>
#include <QXmlStreamReader>

#ifdef HAS_LIBZIP
#include <zip.h>

class DocxParser : public DocumentProcessor {
public:
    DocxParser();
    ~DocxParser() override = default;

    QString name() const override { return "DocxParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    QByteArray readZipEntry(const QString& zipPath, const QString& entryName) const;
    QString parseDocumentXml(const QByteArray& data) const;
    QString parseAppXml(const QByteArray& data) const;
};

#else
// Stub: compile with HAS_LIBZIP to enable DOCX parsing
class DocxParser : public DocumentProcessor {
public:
    DocxParser() = default;
    ~DocxParser() override = default;

    QString name() const override { return "DocxParser (unavailable)"; }
    Result extractText(const QString& /*filePath*/) override {
        Result r; r.success = false; r.errorMessage = "DOCX parsing disabled (requires libzip)"; return r;
    }
    QMap<QString, QString> extractMetadata(const QString& /*filePath*/) override {
        return {};
    }
    bool canProcess(const QString& /*filePath*/) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
#endif

#endif // ANYTXT_DOCX_PARSER_H
