/*
 * rtf_parser.h - RTF 富文本文件解析器
 *
 * 功能说明：解析 .rtf 格式的富文本文档，
 * 通过状态机/正则去除 RTF 控制字提取纯文本内容。
 * 无需外部依赖，纯 Qt6 API 实现。
 */

#ifndef ANYTXT_RTF_PARSER_H
#define ANYTXT_RTF_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>

class RtfParser : public DocumentProcessor {
public:
    RtfParser();
    ~RtfParser() override = default;

    QString name() const override { return "RtfParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    QString extractRtfText(const QByteArray& rtfData) const;
    QString decodeHexChar(const QByteArray& data, int& pos) const;
    QString decodeRtfEncoding(const QByteArray& data, const QString& charset) const;
};

#endif // ANYTXT_RTF_PARSER_H
