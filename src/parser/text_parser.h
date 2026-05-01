/*
 * text_parser.h - 纯文本文件解析器

功能说明：解析 TXT、MD 等纯文本文件，自动检测编码格式。
 */

#ifndef ANYTXT_TEXT_PARSER_H
#define ANYTXT_TEXT_PARSER_H

#include "core/document_processor.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtCore5Compat/QTextCodec>
#else
#include <QTextCodec>
#endif

class TextParser : public DocumentProcessor {
public:
    TextParser();
    ~TextParser() override = default;

    QString name() const override { return "TextParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    QByteArray readFile(const QString& filePath) const;
    QString detectEncoding(const QByteArray& data) const;
    QString decodeText(const QByteArray& data, const QString& encoding) const;
};

#endif // ANYTXT_TEXT_PARSER_H
