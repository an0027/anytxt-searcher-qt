/*
 * wps_parser.cpp - WPS 绯诲垪鏂囦欢瑙ｆ瀽鍣ㄥ疄鐜?(.wps/.et/.dps)
 *
 * 瀹炵幇 WPS Office 鏂囨。鏍煎紡鐨勬枃鏈彁鍙栥€? * WPS 鏂囨。鏈川鏄被 OOXML 鐨?ZIP 鍖咃紝鍐呴儴鍖呭惈 content.xml 鏂囦欢銆? * 璇诲彇 content.xml 瑙ｆ瀽鏂囨湰鍐呭銆? *
 * 鏉′欢缂栬瘧锛氫粎鍦?HAS_LIBZIP 瀹氫箟鏃舵彁渚涘畬鏁村疄鐜般€? */

#include <QDateTime>
#include "parser/wps_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <cstring>

#ifdef HAS_LIBZIP

WpsParser::WpsParser()
{
}

bool WpsParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return (ext == "wps" || ext == "et" || ext == "dps");
}

QStringList WpsParser::supportedExtensions() const
{
    return {"wps", "et", "dps"};
}

QStringList WpsParser::supportedMimeTypes() const
{
    return {
        "application/wps-office.wps",
        "application/wps-office.et",
        "application/wps-office.dps"
    };
}

QByteArray WpsParser::readZipEntry(const QString& zipPath, const QString& entryName) const
{
    QByteArray zipPathSys = QFile::encodeName(zipPath);
    std::string entryNameStd = entryName.toStdString();

    int errCode = 0;
    zip_t* archive = zip_open(zipPathSys.constData(), ZIP_RDONLY, &errCode);
    if (!archive) {
        char errBuf[256];
        zip_error_to_str(errBuf, sizeof(errBuf), errCode, errno);
        qWarning() << "Failed to open zip archive:" << zipPath << errBuf;
        return {};
    }

    zip_stat_t stat;
    if (zip_stat(archive, entryNameStd.c_str(), 0, &stat) != 0) {
        zip_close(archive);
        return {};
    }

    zip_file_t* zfile = zip_fopen(archive, entryNameStd.c_str(), 0);
    if (!zfile) {
        zip_close(archive);
        return {};
    }

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

QString WpsParser::parseContentXmlGeneric(const QByteArray& data) const
{
    // Generic XML text extraction: read all text content from XML elements
    QString text;
    QXmlStreamReader xml(data);

    // Track which elements are text containers vs structural
    // In WPS OOXML-like format, text is typically in <w:t> elements (WPS鏂囧瓧)
    // or <a:t> elements (drawingML for WPS婕旂ず)
    // For <et> spreadsheets, look for <v> and <is> elements
    // Since different WPS formats use different schemas, we do a generic extraction

    QStringList pendingElements; // Stack of parent element names
    bool insideScriptOrStyle = false;

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            QStringView name = xml.name();
            pendingElements.append(name.toString());

            // Skip style metadata elements
            if (name == QStringLiteral("style") || name == QStringLiteral("script")) {
                insideScriptOrStyle = true;
            }
            if (name == QStringLiteral("br")) {
                text += '\n';
            }
            if (name == QStringLiteral("tab")) {
                text += '\t';
            }

            // Paragraph and block-level structure
            if (name == QStringLiteral("p") || name == QStringLiteral("paragraph")) {
                // Check parent hierarchy for newlines
            }

        } else if (token == QXmlStreamReader::Characters) {
            if (!insideScriptOrStyle) {
                QString chunk = xml.text().toString().trimmed();
                if (!chunk.isEmpty()) {
                    text += chunk;
                }
            }
        } else if (token == QXmlStreamReader::EndElement) {
            QStringView name = xml.name();
            if (!pendingElements.isEmpty()) {
                pendingElements.removeLast();
            }
            if (name == QStringLiteral("style") || name == QStringLiteral("script")) {
                insideScriptOrStyle = false;
            }

            // Add newlines after block elements
            if (name == QStringLiteral("p") || name == QStringLiteral("paragraph") ||
                name == QStringLiteral("row") || name == QStringLiteral("tr")) {
                text += '\n';
            } else if (name == QStringLiteral("cell") || name == QStringLiteral("tc") ||
                       name == QStringLiteral("td")) {
                text += '\t';
            }
        }
    }

    if (xml.hasError()) {
        qWarning() << "XML parse error in WPS content:" << xml.errorString();
    }

    // Collapse multiple newlines and trim
    static QRegularExpression multiNewline("\n{3,}");
    text.replace(multiNewline, "\n\n");

    return text.trimmed();
}

WpsParser::Result WpsParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    // Try multiple possible content paths that WPS might use
    QStringList contentPaths = {
        "content.xml",
        "Content.xml",
        "CONTENT.xml",
        "wps/content.xml",
        "et/content.xml",
        "dps/content.xml",
        "word/document.xml",
        "xl/sharedStrings.xml",
        "xl/worksheets/sheet1.xml",
        "ppt/slides/slide1.xml"
    };

    QByteArray contentData;

    // Check if it's a valid ZIP first
    {
        int errCode = 0;
        zip_t* testArchive = zip_open(filePath.toStdString().c_str(), ZIP_RDONLY, &errCode);
        if (!testArchive) {
            result.errorMessage = "WPS file is not a valid ZIP archive: " + filePath;
            return result;
        }
        zip_close(testArchive);
    }

    // Try each content path
    for (const QString& path : contentPaths) {
        contentData = readZipEntry(filePath, path);
        if (!contentData.isEmpty()) {
            qDebug() << "WPS: Found content at" << path;
            break;
        }
    }

    if (contentData.isEmpty()) {
        // Try to list zip contents to find the right file
        qDebug() << "WPS: Trying to enumerate ZIP contents for" << filePath;

        int errCode = 0;
        zip_t* archive = zip_open(filePath.toStdString().c_str(), ZIP_RDONLY, &errCode);
        if (archive) {
            zip_int64_t numEntries = zip_get_num_entries(archive, 0);
            for (zip_int64_t i = 0; i < numEntries; ++i) {
                const char* name = zip_get_name(archive, i, 0);
                if (name) {
                    QString entryName = QString::fromUtf8(name);
                    if (entryName.endsWith("content.xml", Qt::CaseInsensitive) ||
                        entryName.endsWith("/document.xml") ||
                        entryName.contains("sharedStrings")) {
                        contentData = readZipEntry(filePath, entryName);
                        if (!contentData.isEmpty()) {
                            qDebug() << "WPS: Found content at" << entryName;
                            break;
                        }
                    }
                }
            }
            zip_close(archive);
        }
    }

    if (contentData.isEmpty()) {
        result.errorMessage = "WPS: Unable to find content.xml or document content in " + filePath;
        return result;
    }

    // Parse the content
    result.text = parseContentXmlGeneric(contentData);
    result.metadata = extractMetadata(filePath);

    // Try to read core properties
    QByteArray coreXml = readZipEntry(filePath, "docProps/core.xml");
    if (!coreXml.isEmpty()) {
        QXmlStreamReader xml(coreXml);
        while (!xml.atEnd() && !xml.hasError()) {
            xml.readNext();
            if (xml.isStartElement()) {
                QString name = xml.name().toString();
                if (name == "title" || name == "creator" || name == "subject" ||
                    name == "description" || name == "lastModifiedBy") {
                    result.metadata[name] = xml.readElementText();
                }
            }
        }
    }

    // Set appropriate MIME type based on extension
    if (ext == "wps") {
        result.metadata["mimeType"] = "application/wps-office.wps";
    } else if (ext == "et") {
        result.metadata["mimeType"] = "application/wps-office.et";
    } else {
        result.metadata["mimeType"] = "application/wps-office.dps";
    }

    result.success = true;
    return result;
}

QMap<QString, QString> WpsParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = ext;

    if (ext == "wps")
        meta["mimeType"] = "application/wps-office.wps";
    else if (ext == "et")
        meta["mimeType"] = "application/wps-office.et";
    else
        meta["mimeType"] = "application/wps-office.dps";

    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}

#endif // HAS_LIBZIP




