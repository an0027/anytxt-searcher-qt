/*
 * pdf_parser.h - PDF 文件解析器

功能说明：基于 Poppler C++ API 解析 PDF 文件，
提取文本内容和元数据（作者、标题、页数等）。
不依赖 poppler-qt 绑定，直接使用 poppler-cpp。
 */

#ifndef ANYTXT_PDF_PARSER_H
#define ANYTXT_PDF_PARSER_H

#include "core/document_processor.h"

#ifdef HAS_POPPLER
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>

class PdfParser : public DocumentProcessor {
public:
    PdfParser();
    ~PdfParser() override = default;

    QString name() const override { return "PdfParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;
};

#else
// Stub: compile with HAS_POPPLER to enable PDF parsing
class PdfParser : public DocumentProcessor {
public:
    PdfParser() = default;
    ~PdfParser() override = default;

    QString name() const override { return "PdfParser (unavailable)"; }
    Result extractText(const QString& /*filePath*/) override {
        Result r; r.success = false; r.errorMessage = "PDF parsing disabled (requires poppler)"; return r;
    }
    QMap<QString, QString> extractMetadata(const QString& /*filePath*/) override {
        return {};
    }
    bool canProcess(const QString& /*filePath*/) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
#endif

#endif // ANYTXT_PDF_PARSER_H
