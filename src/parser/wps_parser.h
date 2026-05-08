/*
 * wps_parser.h - WPS 系列文件解析器 (.wps/.et/.dps)
 *
 * 功能说明：解析 WPS Office 系列文档格式，
 * 包括 WPS 文字(.wps)、WPS 表格(.et)、WPS 演示(.dps)。
 * 基于 ZIP 和 XML 解析（类似 OOXML 结构）。
 * 条件编译：仅在 HAS_LIBZIP 定义时启用完整实现。
 */

#ifndef ANYTXT_WPS_PARSER_H
#define ANYTXT_WPS_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>
#include <QXmlStreamReader>

#ifdef HAS_LIBZIP
#include <zip.h>

class WpsParser : public DocumentProcessor {
public:
    WpsParser();
    ~WpsParser() override = default;

    QString name() const override { return "WpsParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    QByteArray readZipEntry(const QString& zipPath, const QString& entryName) const;
    QString parseContentXml(const QByteArray& data) const;
    QString parseContentXmlGeneric(const QByteArray& data) const;
};

#else
// Stub: compile with HAS_LIBZIP to enable WPS parsing
class WpsParser : public DocumentProcessor {
public:
    WpsParser() = default;
    ~WpsParser() override = default;

    QString name() const override { return "WpsParser (unavailable)"; }
    Result extractText(const QString& /*filePath*/) override {
        Result r; r.success = false; r.errorMessage = "WPS parsing disabled (requires libzip)"; return r;
    }
    QMap<QString, QString> extractMetadata(const QString& /*filePath*/) override {
        return {};
    }
    bool canProcess(const QString& /*filePath*/) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
#endif

#endif // ANYTXT_WPS_PARSER_H
