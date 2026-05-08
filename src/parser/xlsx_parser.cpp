/*
 * xlsx_parser.cpp - Excel .xlsx 鏂囦欢瑙ｆ瀽鍣? *
 * 璇诲彇 xl/sharedStrings.xml + xl/worksheets/sheet*.xml 鎻愬彇鏂囨湰銆? * 鏉′欢缂栬瘧锛欻AS_LIBZIP
 */

#include <QDateTime>
#include "parser/xlsx_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <cstring>

#ifdef HAS_LIBZIP

XlsxParser::XlsxParser() {}

bool XlsxParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return (ext == "xlsx" || ext == "xlsm");
}

QStringList XlsxParser::supportedExtensions() const
{
    return {"xlsx", "xlsm"};
}

QStringList XlsxParser::supportedMimeTypes() const
{
    return {
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        "application/vnd.ms-excel.sheet.macroEnabled.12"
    };
}

QByteArray XlsxParser::readZipEntry(const QString& zipPath, const QString& entryName) const
{
    QFile file(zipPath);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QByteArray fileData = file.readAll();
    file.close();

    zip_error_t error;
    zip_error_init(&error);

    zip_source_t* src = zip_source_buffer_create(fileData.constData(), fileData.size(), 0, &error);
    if (!src) {
        zip_error_fini(&error);
        return {};
    }

    zip_t* archive = zip_open_from_source(src, ZIP_RDONLY, &error);
    if (!archive) {
        zip_source_free(src);
        zip_error_fini(&error);
        return {};
    }

    std::string entryNameStd = entryName.toStdString();

    zip_stat_t stat;
    if (zip_stat(archive, entryNameStd.c_str(), 0, &stat) != 0) {
        zip_close(archive);
        zip_error_fini(&error);
        return {};
    }

    zip_file_t* zfile = zip_fopen(archive, entryNameStd.c_str(), 0);
    if (!zfile) {
        zip_close(archive);
        zip_error_fini(&error);
        return {};
    }

    QByteArray data;
    data.resize(static_cast<int>(stat.size));
    zip_int64_t bytesRead = zip_fread(zfile, data.data(), stat.size);
    zip_fclose(zfile);
    zip_close(archive);
    zip_error_fini(&error);

    if (bytesRead < 0) return {};
    data.resize(static_cast<int>(bytesRead));
    return data;
}

QStringList XlsxParser::parseSharedStrings(const QByteArray& data) const
{
    QStringList strings;
    if (data.isEmpty()) return strings;

    QXmlStreamReader xml(data);
    while (!xml.atEnd() && !xml.hasError()) {
        if (xml.readNext() == QXmlStreamReader::StartElement) {
            if (xml.name() == QStringLiteral("si")) {
                // Read all <t> text inside this shared string item
                QString itemText;
                int depth = 1;
                while (depth > 0 && !xml.atEnd() && !xml.hasError()) {
                    QXmlStreamReader::TokenType t = xml.readNext();
                    if (t == QXmlStreamReader::StartElement) {
                        if (xml.name() == QStringLiteral("t")) {
                            itemText += xml.readElementText();
                        } else if (xml.name() == QStringLiteral("rPr")) {
                            xml.skipCurrentElement();
                        } else {
                            depth++;
                        }
                    } else if (t == QXmlStreamReader::EndElement) {
                        depth--;
                    }
                }
                strings.append(itemText.trimmed());
            }
        }
    }
    return strings;
}

QString XlsxParser::parseWorksheet(const QByteArray& data, const QStringList& sharedStrings) const
{
    if (data.isEmpty()) return {};

    QString text;
    QXmlStreamReader xml(data);

    while (!xml.atEnd() && !xml.hasError()) {
        QXmlStreamReader::TokenType token = xml.readNext();

        if (token == QXmlStreamReader::StartElement) {
            if (xml.name() == QStringLiteral("v")) {
                // Cell value - numeric or shared string reference
                QString value = xml.readElementText();
                text += value;
                text += QChar('\t');
            } else if (xml.name() == QStringLiteral("is")) {
                // Inline string: <is><t>text</t></is>
                // Read text from all <t> children
                QString inlineText;
                while (!xml.atEnd() && !xml.hasError()) {
                    QXmlStreamReader::TokenType t = xml.readNext();
                    if (t == QXmlStreamReader::StartElement) {
                        if (xml.name() == QStringLiteral("t")) {
                            inlineText += xml.readElementText();
                        } else if (xml.name() == QStringLiteral("rPr")) {
                            xml.skipCurrentElement();
                        }
                    } else if (t == QXmlStreamReader::EndElement) {
                        if (xml.name() == QStringLiteral("is")) break;
                    }
                }
                text += inlineText;
                text += QChar('\t');
            } else if (xml.name() == QStringLiteral("t")) {
                text += xml.readElementText();
            }
        } else if (token == QXmlStreamReader::EndElement) {
            if (xml.name() == QStringLiteral("row")) {
                if (text.endsWith(QChar('\t'))) text.chop(1);
                text += QChar('\n');
            }
        }
    }

    return text.trimmed();
}

QStringList XlsxParser::enumerateWorksheetPaths(const QString& zipPath) const
{
    QStringList sheets;

    // Read relationships to find sheet -> file mapping
    QByteArray relsXml = readZipEntry(zipPath, QStringLiteral("xl/_rels/workbook.xml.rels"));
    if (relsXml.isEmpty()) {
        // Fallback: try numbered sheet paths
        for (int i = 1; i <= 20; ++i) {
            QString path = QStringLiteral("xl/worksheets/sheet%1.xml").arg(i);
            if (!readZipEntry(zipPath, path).isEmpty())
                sheets << path;
            else if (i > 1) break; // Stop after first gap
        }
        return sheets;
    }

    // Build relationship map: rId -> Target
    QMap<QString, QString> relMap;
    QXmlStreamReader rels(relsXml);
    while (!rels.atEnd() && !rels.hasError()) {
        if (rels.readNext() == QXmlStreamReader::StartElement) {
            if (rels.name() == QStringLiteral("Relationship")) {
                QString id = rels.attributes().value(QStringLiteral("Id")).toString();
                QString target = rels.attributes().value(QStringLiteral("Target")).toString();
                if (!id.isEmpty() && !target.isEmpty())
                    relMap[id] = target;
            }
        }
    }

    // Read workbook to find sheet rIds
    QByteArray wbXml = readZipEntry(zipPath, QStringLiteral("xl/workbook.xml"));
    if (wbXml.isEmpty()) {
        for (int i = 1; i <= 20; ++i) {
            QString path = QStringLiteral("xl/worksheets/sheet%1.xml").arg(i);
            if (!readZipEntry(zipPath, path).isEmpty())
                sheets << path;
            else if (i > 1) break;
        }
        return sheets;
    }

    QXmlStreamReader wb(wbXml);
    while (!wb.atEnd() && !wb.hasError()) {
        if (wb.readNext() == QXmlStreamReader::StartElement) {
            if (wb.name() == QStringLiteral("sheet")) {
                QString rId = wb.attributes().value(
                    QStringLiteral("http://schemas.openxmlformats.org/officeDocument/2006/relationships"),
                    QStringLiteral("id")
                ).toString();
                if (rId.isEmpty())
                    rId = wb.attributes().value(QStringLiteral("r:id")).toString();

                if (relMap.contains(rId)) {
                    QString target = relMap[rId];
                    // Strip leading slash (some Excel versions produce absolute paths)
                    if (target.startsWith(QChar('/'))) target = target.mid(1);
                    // Ensure correct path prefix
                    if (!target.startsWith(QStringLiteral("xl/worksheets/")) &&
                        !target.startsWith(QStringLiteral("worksheets/"))) {
                        target = QStringLiteral("xl/worksheets/") + target;
                    }
                    sheets << target;
                }
            }
        }
    }

    return sheets;
}

XlsxParser::Result XlsxParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // Read shared strings
    QByteArray ssData = readZipEntry(filePath, QStringLiteral("xl/sharedStrings.xml"));
    QStringList sharedStrings = parseSharedStrings(ssData);

    // Find and parse worksheets
    QStringList sheetPaths = enumerateWorksheetPaths(filePath);
    QString fullText;

    for (const QString& sheetPath : sheetPaths) {
        QByteArray sheetData = readZipEntry(filePath, sheetPath);
        if (sheetData.isEmpty()) continue;

        QString sheetText = parseWorksheet(sheetData, sharedStrings);
        if (!sheetText.isEmpty()) {
            if (!fullText.isEmpty()) fullText += QStringLiteral("\n\n");
            fullText += sheetText;
        }

        if (fullText.length() > 500000) break; // safety limit
    }

    result.text = fullText;
    result.metadata = extractMetadata(filePath);
    result.metadata[QStringLiteral("mimeType")] =
        QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet");
    result.success = !fullText.isEmpty();
    return result;
}

QMap<QString, QString> XlsxParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);
    meta[QStringLiteral("fileSize")] = QString::number(fi.size());
    meta[QStringLiteral("modifiedTime")] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta[QStringLiteral("fileExt")] = fi.suffix().toLower();
    meta[QStringLiteral("fileName")] = fi.fileName();
    meta[QStringLiteral("filePath")] = fi.absoluteFilePath();
    return meta;
}

#endif // HAS_LIBZIP




