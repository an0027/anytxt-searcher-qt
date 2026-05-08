/*
 * eml_parser.cpp - 閭欢 .eml 鏂囦欢瑙ｆ瀽鍣ㄥ疄鐜? *
 * 瀹炵幇 .eml 鏍煎紡閭欢鐨勮В鏋愶紝鎻愬彇澶撮儴淇℃伅锛團rom/To/Subject/Date锛? * 鍜屾鏂囧唴瀹广€傛敮鎸?MIME multipart 娑堟伅锛宼ext/plain 浼樺厛锛? * text/html 闄嶇骇涓虹函鏂囨湰銆? *
 * 绾?Qt6 API锛屾棤澶栭儴渚濊禆銆? */

#include <QDateTime>
#include "parser/eml_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QRegularExpression>
#include <QUrl>

EmlParser::EmlParser()
{
}

bool EmlParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == "eml";
}

QStringList EmlParser::supportedExtensions() const
{
    return {"eml"};
}

QStringList EmlParser::supportedMimeTypes() const
{
    return {"message/rfc822", "text/plain"};
}

// Decode MIME header encoding =?charset?encoding?text?=
QString EmlParser::decodeMimeHeader(const QString& header) const
{
    static QRegularExpression mimeRegex("=\\?([^?]+)\\?([BbQq])\\?([^?]*)\\?=");
    QString result = header;

    // First, fold continuation lines (whitespace+newline at start of continuation)
    result.replace(QRegularExpression("\\r?\\n\\s+"), " ");

    // Decode each encoded word
    int pos = 0;
    QRegularExpressionMatchIterator it = mimeRegex.globalMatch(result);
    QList<QPair<int, int>> ranges;
    QList<QString> decoded;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString charset = match.captured(1).toLower();
        QString encoding = match.captured(2).toUpper();
        QString text = match.captured(3);

        QByteArray rawData = text.toLatin1();
        QString decodedText;

        if (encoding == "B") {
            decodedText = decodeBase64(rawData);
        } else if (encoding == "Q") {
            decodedText = decodeQuotedPrintable(rawData);
        }

        // Try to convert from the specified charset
        if (!charset.isEmpty() && charset != "utf-8" && charset != "us-ascii") {
            auto decoder = QStringDecoder(charset.toStdString().c_str());
            if (decoder.isValid()) {
                QByteArray rawBytes = decodedText.toLatin1();
                decodedText = decoder(rawBytes);
            }
        }

        ranges.append({match.capturedStart(), match.capturedEnd()});
        decoded.append(decodedText);
    }

    // Apply replacements from right to left to preserve positions
    for (int i = ranges.size() - 1; i >= 0; --i) {
        result.replace(ranges[i].first, ranges[i].second - ranges[i].first, decoded[i]);
    }

    return result.trimmed();
}

QString EmlParser::decodeQuotedPrintable(const QByteArray& data) const
{
    QByteArray result;
    result.reserve(data.size());

    for (int i = 0; i < data.size(); ++i) {
        if (data[i] == '=' && i + 2 < data.size()) {
            // Soft line break (=\r\n or =\n) - skip
            if (data[i + 1] == '\r' && data[i + 2] == '\n') {
                i += 2;
                continue;
            }
            if (data[i + 1] == '\n') {
                i += 1;
                continue;
            }

            // Hex escape =XX
            char hex[3] = {static_cast<char>(data[i + 1]), static_cast<char>(data[i + 2]), '\0'};
            char* endptr = nullptr;
            long val = strtol(hex, &endptr, 16);
            if (endptr == hex + 2) {
                result.append(static_cast<char>(val));
                i += 2;
            } else {
                result.append(data[i]);
            }
        } else if (data[i] == '_' && false) {
            // In Q-encoding, _ represents space (only applies to MIME encoded words, handled above)
            result.append(' ');
        } else {
            result.append(data[i]);
        }
    }

    return QString::fromUtf8(result);
}

QString EmlParser::decodeBase64(const QByteArray& data) const
{
    return QString::fromUtf8(QByteArray::fromBase64(data));
}

QString EmlParser::extractTextFromHtmlSimple(const QString& html) const
{
    QString text = html;

    // Remove scripts and styles
    static QRegularExpression scriptTag("<script[^>]*>[\\s\\S]*?</script>",
        QRegularExpression::CaseInsensitiveOption);
    text.remove(scriptTag);
    static QRegularExpression styleTag("<style[^>]*>[\\s\\S]*?</style>",
        QRegularExpression::CaseInsensitiveOption);
    text.remove(styleTag);

    // Replace common block tags with newlines
    text.replace(QRegularExpression("</?(p|div|br|tr|li|h[1-6]|blockquote|pre|hr|table)[^>]*>",
        QRegularExpression::CaseInsensitiveOption), "\n");

    // Replace list items with bullet
    text.replace(QRegularExpression("<li[^>]*>",
        QRegularExpression::CaseInsensitiveOption), "\n  \u2022 ");

    // Replace table cells with tabs
    text.replace(QRegularExpression("</?(td|th)[^>]*>",
        QRegularExpression::CaseInsensitiveOption), "\t");

    // Remove all remaining HTML tags
    static QRegularExpression htmlTag("<[^>]*>");
    text.remove(htmlTag);

    // Decode HTML entities
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&nbsp;", " ");
    text.replace("&#160;", " ");
    // Decode numeric HTML entities
        // Decode numeric HTML entities using string-based approach
    {
        int pos = 0;
        QRegularExpression entityRe(QStringLiteral("&#(\\d+);"));
        QRegularExpressionMatch m;
        while ((m = entityRe.match(text, pos)).hasMatch()) {
            bool ok = false;
            int code = m.captured(1).toInt(&ok);
            if (ok && code > 0) {
                text.replace(m.capturedStart(), m.capturedLength(), QChar(code));
            }
            pos = m.capturedStart() + 1;
        }
    }

    // Collapse excessive whitespace
    static QRegularExpression multiNewline("\n{3,}");
    text.replace(multiNewline, "\n\n");
    static QRegularExpression trailingSpace(" +\n");
    text.replace(trailingSpace, "\n");

    return text.trimmed();
}

QStringList EmlParser::splitMimeParts(const QByteArray& data, const QByteArray& boundary) const
{
    QStringList parts;
    QByteArray boundaryMarker = "--" + boundary;
    QByteArray endMarker = boundaryMarker + "--";

    QList<QByteArray> rawParts;
    int start = data.indexOf(boundaryMarker);
    if (start < 0) {
        // No boundary found - return the whole data as one part
        return {QString::fromUtf8(data)};
    }

    // Move past the first boundary
    int searchStart = start + boundaryMarker.size();
    int nextBoundary;

    while ((nextBoundary = data.indexOf(boundaryMarker, searchStart)) >= 0) {
        QByteArray part = data.mid(searchStart, nextBoundary - searchStart);
        rawParts.append(part.trimmed());
        searchStart = nextBoundary + boundaryMarker.size();

        // Check for end marker
        if (data.mid(nextBoundary, endMarker.size()) == endMarker) {
            break;
        }
    }

    for (const QByteArray& rawPart : rawParts) {
        parts.append(QString::fromUtf8(rawPart));
    }

    return parts;
}

EmlParser::EmailData EmlParser::parseEml(const QByteArray& data) const
{
    EmailData email;
    QString content = QString::fromUtf8(data);

    // Split headers and body at first empty line
    int headerEnd = content.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        headerEnd = content.indexOf("\n\n");
    }
    if (headerEnd < 0) {
        email.success = false;
        return email;
    }

    QString headerSection = content.left(headerEnd);
    QString bodySection = content.mid(headerEnd + (content.indexOf("\r\n\r\n") >= 0 ? 4 : 2));

    QByteArray bodyRaw = data.mid(headerEnd + (data.indexOf("\r\n\r\n") >= 0 ? 4 : 2));

    // Parse headers
    QStringList headerLines = headerSection.split('\n');
    QString currentHeader;
    QString currentValue;

    for (int i = 0; i < headerLines.size(); ++i) {
        QString line = headerLines[i].trimmed();
        if (line.isEmpty()) continue;

        if (line[0].isSpace() || line[0] == '\t') {
            // Continuation of previous header
            // Folded header lines start with whitespace
            currentValue += " " + line.trimmed();
        } else {
            // Save previous header
            if (!currentHeader.isEmpty()) {
                email.headers[currentHeader.toLower()] = currentValue.trimmed();
            }

            // Parse new header
            int colonPos = line.indexOf(':');
            if (colonPos >= 0) {
                currentHeader = line.left(colonPos).trimmed();
                currentValue = line.mid(colonPos + 1).trimmed();
            }
        }
    }

    // Save last header
    if (!currentHeader.isEmpty()) {
        email.headers[currentHeader.toLower()] = currentValue.trimmed();
    }

    // Determine content type
    QString contentType = email.headers.value("content-type", "text/plain").toLower();

    // Check for multipart
    static QRegularExpression boundaryRegex("boundary=\"?([^\";]+)\"?",
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch boundaryMatch = boundaryRegex.match(contentType);

    if (boundaryMatch.hasMatch()) {
        QByteArray boundary = boundaryMatch.captured(1).toLatin1();
        QStringList parts = splitMimeParts(bodyRaw, boundary);

        // Find the first text/plain or text/html part
        QString bestText;

        for (const QString& part : parts) {
            if (part.isEmpty()) continue;

            // Parse part headers
            int partHeaderEnd = part.indexOf("\n\n");
            if (partHeaderEnd < 0) continue;

            QString partHeaders = part.left(partHeaderEnd).trimmed();
            QString partBody = part.mid(partHeaderEnd + 2).trimmed();

            QString partContentType;
            QString partEncoding;

            QStringList pHLines = partHeaders.split('\n');
            for (const QString& hl : pHLines) {
                QString hlTrimmed = hl.trimmed();
                if (hlTrimmed.startsWith("Content-Type:", Qt::CaseInsensitive)) {
                    partContentType = hlTrimmed.mid(13).trimmed().toLower();
                } else if (hlTrimmed.startsWith("Content-Transfer-Encoding:", Qt::CaseInsensitive)) {
                    partEncoding = hlTrimmed.mid(26).trimmed().toLower();
                }
            }

            QByteArray partData = partBody.toUtf8();
            QString decoded;

            if (partEncoding == "base64") {
                decoded = decodeBase64(partData);
            } else if (partEncoding == "quoted-printable") {
                decoded = decodeQuotedPrintable(partData);
            } else {
                decoded = partBody;
            }

            if (partContentType.startsWith("text/plain")) {
                bestText = decoded;
                break;  // text/plain is preferred
            } else if (partContentType.startsWith("text/html") && bestText.isEmpty()) {
                bestText = extractTextFromHtmlSimple(decoded);
            }
        }

        email.body = bestText;
    } else if (contentType.startsWith("text/html")) {
        QString transferEncoding = email.headers.value("content-transfer-encoding", "").toLower();
        QString decoded;
        if (transferEncoding == "base64") {
            decoded = decodeBase64(bodyRaw);
        } else if (transferEncoding == "quoted-printable") {
            decoded = decodeQuotedPrintable(bodyRaw);
        } else {
            decoded = bodySection;
        }
        email.body = extractTextFromHtmlSimple(decoded);
    } else {
        // text/plain or unknown
        QString transferEncoding = email.headers.value("content-transfer-encoding", "").toLower();
        if (transferEncoding == "base64") {
            email.body = decodeBase64(bodyRaw);
        } else if (transferEncoding == "quoted-printable") {
            email.body = decodeQuotedPrintable(bodyRaw);
        } else {
            email.body = bodySection;
        }
    }

    // Decode headers
    for (auto it = email.headers.begin(); it != email.headers.end(); ++it) {
        it.value() = decodeMimeHeader(it.value());
    }

    email.success = true;
    return email;
}

EmlParser::Result EmlParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // Read the file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = "Failed to open .eml file: " + file.errorString();
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        result.errorMessage = "EML file is empty";
        return result;
    }

    // Parse the email
    EmailData email = parseEml(data);
    if (!email.success) {
        result.errorMessage = "Failed to parse EML file";
        return result;
    }

    // Build full text with headers
    QString fullText;

    // Add metadata-like header information
    fullText += "From: " + email.headers.value("from", "(unknown)") + "\n";
    fullText += "To: " + email.headers.value("to", "(unknown)") + "\n";
    fullText += "Subject: " + email.headers.value("subject", "(no subject)") + "\n";
    fullText += "Date: " + email.headers.value("date", "(no date)") + "\n";

    if (email.headers.contains("cc")) {
        fullText += "Cc: " + email.headers.value("cc") + "\n";
    }

    // Add body
    fullText += "\n" + email.body;

    result.text = fullText;
    result.metadata = extractMetadata(filePath);

    // Copy headers to metadata
    const QStringList metaKeys = {"from", "to", "subject", "date", "cc", "message-id",
                                  "in-reply-to", "references", "priority", "x-priority"};
    for (const QString& key : metaKeys) {
        if (email.headers.contains(key)) {
            result.metadata[key] = email.headers.value(key);
        }
    }

    result.metadata["mimeType"] = "message/rfc822";
    result.success = true;

    return result;
}

QMap<QString, QString> EmlParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = "eml";
    meta["mimeType"] = "message/rfc822";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}



