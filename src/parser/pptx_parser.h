#pragma once
#include "core/document_processor.h"

class PptxParser : public DocumentProcessor {
public:
    PptxParser() = default;
    QString name() const override { return QStringLiteral("X (unavailable)"); }
    Result extractText(const QString&) override { Result r; r.success = false; r.errorMessage = "Parser not available"; return r; }
    QMap<QString, QString> extractMetadata(const QString&) override { return {}; }
    bool canProcess(const QString&) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
