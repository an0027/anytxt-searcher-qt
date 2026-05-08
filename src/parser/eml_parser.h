/*
 * eml_parser.h - 邮件 .eml 文件解析器
 *
 * 功能说明：解析 .eml 格式的邮件文件，
 * 读取邮件头部（From/To/Subject/Date）和正文内容。
 * 处理 MIME multipart（只取 text/plain 或 text/html 部分）。
 * 无需外部依赖，纯 Qt6 API 实现。
 */

#ifndef ANYTXT_EML_PARSER_H
#define ANYTXT_EML_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>
#include <QMap>

class EmlParser : public DocumentProcessor {
public:
    EmlParser();
    ~EmlParser() override = default;

    QString name() const override { return "EmlParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    struct EmailData {
        QMap<QString, QString> headers;
        QString body;
        bool success = false;
    };

    EmailData parseEml(const QByteArray& data) const;
    QString decodeMimeHeader(const QString& header) const;
    QString decodeQuotedPrintable(const QByteArray& data) const;
    QString decodeBase64(const QByteArray& data) const;
    QString extractTextFromHtmlSimple(const QString& html) const;
    QStringList splitMimeParts(const QByteArray& data, const QByteArray& boundary) const;
};

#endif // ANYTXT_EML_PARSER_H
