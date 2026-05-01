/*
 * docx_parser.cpp - DOCX 文件解析器实现

实现 DOCX 文档的 ZIP 解压、XML 解析和文本提取。
条件编译：仅在 HAS_LIBZIP 定义时提供完整实现。
 */

#include <QDateTime>
#include "parser/docx_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <cstring>

#ifdef HAS_LIBZIP

DocxParser::DocxParser()
{
}

bool DocxParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == "docx";
}

QStringList DocxParser::supportedExtensions() const
{
    return {"docx", "docm"};
}

QStringList DocxParser::supportedMimeTypes() const
{
    return {
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
        "application/vnd.ms-word.document.macroEnabled.12"
    };
}

QByteArray DocxParser::readZipEntry(const QString& zipPath, const QString& entryName) const
{
    std::string zipPathStd = zipPath.toStdString();
    std::string entryNameStd = entryName.toStdString();

    // Open the zip archive
    int errCode = 0;
    zip_t* archive = zip_open(zipPathStd.c_str(), ZIP_RDONLY, &errCode);
    if (!archive) {
        char errBuf[256];
        zip_error_to_str(errBuf, sizeof(errBuf), errCode, errno);
        qWarning() << "Failed to open zip archive:" << zipPath << errBuf;
        return {};
    }

    // Find the entry
    zip_stat_t stat;
    if (zip_stat(archive, entryNameStd.c_str(), 0, &stat) != 0) {
        qWarning() << "Entry not found in zip:" << entryName;
        zip_close(archive);
        return {};
    }

    // Open the entry
    zip_file_t* zfile = zip_fopen(archive, entryNameStd.c_str(), 0);
    if (!zfile) {
        qWarning() << "Failed to open entry in zip:" << entryName;
        zip_close(archive);
        return {};
    }

    // Read the data
    QByteArray data;
    data.resize(static_cast<int>(stat.size));
    zip_int64_t bytesRead = zip_fread(zfile, data.data(), stat.size);

    zip_fclose(zfile);
    zip_close(archive);

    if (bytesRead < 0) {
        qWarning() << "Failed to read entry:" << entryName;
        return {};
    }

    data.resize(static_cast<int>(bytesRead));
    return data;
}

QString DocxParser::parseDocumentXml(const QByteArray& data) const
{
    QString text;
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            // Look for <w:t> elements which contain text
            if (xml.name() == QStringLiteral("t") &&
                xml.namespaceUri() == QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) {
                text += xml.readElementText();
            }
            // <w:br/> for line breaks
            if (xml.name() == QStringLiteral("br") &&
                xml.namespaceUri() == QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) {
                text += '\n';
            }
        } else if (token == QXmlStreamReader::EndElement) {
            // <w:p> paragraphs - add newline when a paragraph ends
            if (xml.name() == QStringLiteral("p") &&
                xml.namespaceUri() == QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) {
                text += '\n';
            }
            // Table cell end
            if (xml.name() == QStringLiteral("tc") &&
                xml.namespaceUri() == QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) {
                text += '\t';
            }
            // Table row end
            if (xml.name() == QStringLiteral("tr") &&
                xml.namespaceUri() == QStringLiteral("http://schemas.openxmlformats.org/wordprocessingml/2006/main")) {
                text += '\n';
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "XML parse error in docx document:" << xml.errorString();
    }

    return text.trimmed();
}

QString DocxParser::parseAppXml(const QByteArray& data) const
{
    QString metadata;
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        xml.readNext();
    }

    return metadata;
}

DocxParser::Result DocxParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // Read the document.xml from the docx archive
    QByteArray docXml = readZipEntry(filePath, "word/document.xml");
    if (docXml.isEmpty()) {
        result.errorMessage = "Failed to read word/document.xml from " + filePath;
        qWarning() << result.errorMessage;
        return result;
    }

    result.text = parseDocumentXml(docXml);
    result.metadata = extractMetadata(filePath);

    // Try to read additional metadata from docProps
    QByteArray appXml = readZipEntry(filePath, "docProps/app.xml");
    if (!appXml.isEmpty()) {
        parseAppXml(appXml);
    }

    // Try to read core properties for more metadata
    QByteArray coreXml = readZipEntry(filePath, "docProps/core.xml");
    if (!coreXml.isEmpty()) {
        QXmlStreamReader xml(coreXml);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (xml.isStartElement()) {
                QString name = xml.name().toString();
                QString ns = xml.namespaceUri().toString();
                Q_UNUSED(ns);
                if (name == "title" || name == "creator" || name == "subject" ||
                    name == "description" || name == "keywords" ||
                    name == "lastModifiedBy" || name == "revision") {
                    result.metadata[name] = xml.readElementText();
                }
            }
        }
    }

    result.metadata["mimeType"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    result.success = true;

    return result;
}

QMap<QString, QString> DocxParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = "docx";
    meta["mimeType"] = "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}

#endif // HAS_LIBZIP
