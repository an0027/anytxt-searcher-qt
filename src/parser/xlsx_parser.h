/*
 * xlsx_parser.h - Excel .xlsx 文件解析器
 *
 * 功能说明：解析 .xlsx/.xlsm 格式的 Excel 文档，
 * 基于 ZIP 和 XML 解析提取文本内容（共享字符串表 + 工作表）。
 * 条件编译：仅在 HAS_LIBZIP 定义时启用完整实现。
 */

#ifndef ANYTXT_XLSX_PARSER_H
#define ANYTXT_XLSX_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>
#include <QXmlStreamReader>

#ifdef HAS_LIBZIP
#include <zip.h>

class XlsxParser : public DocumentProcessor {
public:
    XlsxParser();
    ~XlsxParser() override = default;

    QString name() const override { return "XlsxParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    QByteArray readZipEntry(const QString& zipPath, const QString& entryName) const;
    QStringList parseSharedStrings(const QByteArray& data) const;
    QString parseWorksheet(const QByteArray& data, const QStringList& sharedStrings) const;
    QStringList enumerateWorksheetPaths(const QString& zipPath) const;
    QString parseStylesXml(const QByteArray& data) const;
};

#else
// Stub: compile with HAS_LIBZIP to enable XLSX parsing
class XlsxParser : public DocumentProcessor {
public:
    XlsxParser() = default;
    ~XlsxParser() override = default;

    QString name() const override { return "XlsxParser (unavailable)"; }
    Result extractText(const QString& /*filePath*/) override {
        Result r; r.success = false; r.errorMessage = "XLSX parsing disabled (requires libzip)"; return r;
    }
    QMap<QString, QString> extractMetadata(const QString& /*filePath*/) override {
        return {};
    }
    bool canProcess(const QString& /*filePath*/) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
#endif

#endif // ANYTXT_XLSX_PARSER_H
