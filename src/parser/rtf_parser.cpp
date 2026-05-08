/*
 * rtf_parser.cpp - RTF 富文本文件解析器实现
 *
 * 实现 .rtf 格式的富文本文档解析，通过状态机去除 RTF 控制字
 * 和控制符号，提取纯文本内容。
 *
 * 纯 Qt6 API，无外部依赖。
 */

#include <QDateTime>
#include "parser/rtf_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QStringDecoder>
#include <QRegularExpression>

RtfParser::RtfParser()
{
}

bool RtfParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == "rtf";
}

QStringList RtfParser::supportedExtensions() const
{
    return {"rtf"};
}

QStringList RtfParser::supportedMimeTypes() const
{
    return {"application/rtf", "text/rtf"};
}

QString RtfParser::decodeHexChar(const QByteArray& data, int& pos) const
{
    if (pos + 2 >= data.size()) return {};

    char hex[3] = {data[pos], data[pos + 1], '\0'};
    char* endptr = nullptr;
    long val = strtol(hex, &endptr, 16);
    if (endptr == hex + 2) {
        pos += 2;
        return QChar(static_cast<ushort>(val));
    }
    return {};
}

QString RtfParser::extractRtfText(const QByteArray& rtfData) const
{
    QString text;
    int pos = 0;
    int len = rtfData.size();

    // RTF state
    bool inGroup = false;        // We're inside a group {...}
    bool inControlWord = false;  // Currently parsing a control word
    bool inControlSymbol = false;
    bool inHex = false;
    bool inBinary = false;
    bool inIgnorable = false;    // \* (destinations that should be skipped)
    int groupDepth = 0;

    QString currentControl;
    bool hasParam = false;
    int currentParam = 0;
    bool paramNeg = false;

    // Stack of skip states for nested groups
    QVector<int> skipStack;

    while (pos < len) {
        char ch = rtfData[pos];

        if (inBinary) {
            // Binary data - skip
            if (currentParam > 0) {
                currentParam--;
                pos++;
                if (currentParam == 0) inBinary = false;
                continue;
            }
            inBinary = false;
        }

        if (inHex) {
            // Read two hex chars
            QString hexChar = decodeHexChar(rtfData, pos);
            if (!hexChar.isEmpty()) {
                text += hexChar;
            } else {
                pos++;
            }
            inHex = false;
            continue;
        }

        if (inControlWord || inControlSymbol) {
            if (inControlWord) {
                if (ch >= 'a' && ch <= 'z') {
                    currentControl += ch;
                    pos++;
                    continue;
                } else if (ch >= 'A' && ch <= 'Z') {
                    // Capital letters in control words are usually control symbols
                    currentControl += ch;
                    pos++;

                    // A capital letter control word has no parameter, end it here
                    // Actually RTF control words are lowercase, uppercase means control symbol
                    // Let's handle this
                } else if (ch == '-' || (ch >= '0' && ch <= '9')) {
                    // Parameter parsing
                    hasParam = true;
                    if (ch == '-') {
                        paramNeg = true;
                        pos++;
                        continue;
                    }
                    currentParam = currentParam * 10 + (ch - '0');
                    pos++;
                    continue;
                } else if (ch == ' ') {
                    // Space delimiter - consume the space
                    pos++;
                }
                // End of control word
            }

            if (inControlSymbol) {
                // Single character control symbol - e.g., \~, \*, \-, \_
                // The character has already been read
            }

            // Process the control word
            QString ctrl = currentControl;

            if (hasParam || inControlSymbol) {
                // 'xx hex escape
                if (ctrl == "'" || (inControlSymbol && currentControl.isEmpty())) {
                    if (inControlSymbol && currentControl.isEmpty() && ch != '\'') {
                        // Single char control symbol
                        char sym = rtfData[pos];
                        pos++;
                        // Handle specific symbols
                        switch (sym) {
                            case '~': text += QChar(0xA0); break;  // non-breaking space
                            case '-': text += '-'; break;          // optional hyphen
                            case '_': text += QChar(0x2011); break; // non-breaking hyphen
                            case '*': inIgnorable = true; break;   // ignorable destination
                            case ':': break;  // subentry in index (skip)
                            default: break;
                        }
                    } else if (pos + 2 < len && rtfData[pos] == '\'') {
                        pos++; // skip \'
                        QString hexChar = decodeHexChar(rtfData, pos);
                        if (!hexChar.isEmpty()) {
                            // Convert from current code page - for now assume Latin-1/Western
                            // For proper conversion we'd need to track \ansicpgN
                            // Simple approach: use the raw byte
                            QByteArray raw;
                            raw.append(static_cast<char>(hexChar[0].unicode()));
                            text += QString::fromLatin1(raw);
                        }
                    }
                } else if (ctrl == "par" || ctrl == "pard") {
                    text += '\n';
                } else if (ctrl == "line") {
                    text += '\n';
                } else if (ctrl == "tab") {
                    text += '\t';
                } else if (ctrl == "cell") {
                    text += '\t';
                } else if (ctrl == "row") {
                    text += '\n';
                } else if (ctrl == "sect" || ctrl == "sectd") {
                    text += "\n\n";
                } else if (ctrl == "page" || ctrl == "pagebb") {
                    text += "\n\n";
                } else if (ctrl == "bin") {
                    // Binary data follows, param is byte count
                    if (hasParam && currentParam > 0) {
                        inBinary = true;
                        // currentParam will be decremented as we skip bytes
                    }
                }
            } else {
                // Control word without parameter
                if (ctrl == "par" || ctrl == "pard") {
                    text += '\n';
                } else if (ctrl == "line") {
                    text += '\n';
                } else if (ctrl == "tab") {
                    text += '\t';
                } else if (ctrl == "cell") {
                    text += '\t';
                } else if (ctrl == "row") {
                    text += '\n';
                } else if (ctrl == "~") {
                    text += ' ';
                } else if (ctrl == "\\") {
                    // Escaped backslash - this shouldn't happen at this level
                } else if (ctrl == "*") {
                    inIgnorable = true;
                } else if (ctrl == "fonttbl" || ctrl == "colortbl" ||
                           ctrl == "stylesheet" || ctrl == "listtable" ||
                           ctrl == "listoverridetable" || ctrl == "themedata" ||
                           ctrl == "colorschememapping" || ctrl == "datastore" ||
                           ctrl == "datafield") {
                    inIgnorable = true;
                } else if (ctrl == "ansicpg" || ctrl == "deff" ||
                           ctrl == "fonttbl" || ctrl == "f" ||
                           ctrl == "fs" || ctrl == "cf" ||
                           ctrl == "cb" || ctrl == "highlight" ||
                           ctrl == "b" || ctrl == "i" || ctrl == "u" ||
                           ctrl == "strike" || ctrl == "ul" ||
                           ctrl == "super" || ctrl == "sub" ||
                           ctrl == "qc" || ctrl == "ql" || ctrl == "qr" ||
                           ctrl == "qj" || ctrl == "lang" ||
                           ctrl == "rtf" || ctrl == "ansi" ||
                           ctrl == "mac" || ctrl == "pc" || ctrl == "pca" ||
                           ctrl == "deflang") {
                    // Formatting control words - skip them
                }
            }

            inControlWord = false;
            inControlSymbol = false;
            currentControl.clear();
            hasParam = false;
            currentParam = 0;
            paramNeg = false;
            continue;
        }

        switch (ch) {
            case '{':
                // Open group
                if (inIgnorable) {
                    skipStack.append(1);  // track skip nesting
                }
                groupDepth++;
                inGroup = true;
                inIgnorable = false;
                pos++;
                break;

            case '}':
                // Close group
                groupDepth--;
                if (groupDepth == 0) {
                    inGroup = false;
                }
                // Pop skip stack if needed
                if (!skipStack.isEmpty()) {
                    skipStack.pop_back();
                    if (skipStack.isEmpty()) {
                        inIgnorable = false;
                    }
                }
                inIgnorable = false;
                currentControl.clear();
                hasParam = false;
                currentParam = 0;
                pos++;
                break;

            case '\\':
                // Start of control sequence
                pos++;
                if (pos < len) {
                    char next = rtfData[pos];
                    if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
                        inControlWord = true;
                        currentControl.clear();
                        hasParam = false;
                        currentParam = 0;
                        paramNeg = false;
                    } else if (next == '\'') {
                        // Hex escape \'xx
                        pos++;
                        if (pos + 1 < len) {
                            QString hexChar = decodeHexChar(rtfData, pos);
                            if (!hexChar.isEmpty()) {
                                // For now assume CP-1252 / Latin-1 encoding
                                // In production we'd track the current code page
                                text += hexChar;
                            }
                        }
                    } else {
                        // Control symbol (single non-letter char)
                        // Check if we should skip due to ignorable group
                        char sym = rtfData[pos];
                        pos++;

                        if (skipStack.isEmpty() || !inIgnorable) {
                            switch (sym) {
                                case '~': text += ' '; break;
                                case '-': break;  // optional hyphen
                                case '_': text += '-'; break;
                                case '*': inIgnorable = true; break;
                                case ':': break;
                                case '\\': text += '\\'; break;
                                case '{': text += '{'; break;
                                case '}': text += '}'; break;
                                default: break;
                            }
                        }
                    }
                }
                break;

            case '\r':
            case '\n':
                // Skip carriage returns and newlines in RTF data
                pos++;
                break;

            case '\t':
                text += '\t';
                pos++;
                break;

            default:
                // Regular text characters - only add if not in ignorable group
                if (skipStack.isEmpty() || !inIgnorable) {
                    if (ch >= ' ' && ch <= '~') {
                        // Printable ASCII
                        text += QChar(ch);
                    } else if (static_cast<unsigned char>(ch) >= 0x80) {
                        // Extended ASCII / UTF-8 multibyte
                        QByteArray raw;
                        raw.append(ch);
                        // Try UTF-8 by checking for continuation bytes
                        bool maybeUtf8 = false;
                        if ((static_cast<unsigned char>(ch) & 0xE0) == 0xC0) {
                            // 2-byte UTF-8 sequence
                            if (pos + 1 < len) {
                                raw.append(rtfData[pos + 1]);
                                QString utf8Str = QString::fromUtf8(raw);
                                if (!utf8Str.isEmpty() && utf8Str.size() == 1) {
                                    text += utf8Str;
                                    pos++;
                                    maybeUtf8 = true;
                                }
                            }
                        } else if ((static_cast<unsigned char>(ch) & 0xF0) == 0xE0) {
                            // 3-byte UTF-8 sequence
                            if (pos + 2 < len) {
                                raw.append(rtfData[pos + 1]);
                                raw.append(rtfData[pos + 2]);
                                QString utf8Str = QString::fromUtf8(raw);
                                if (!utf8Str.isEmpty() && utf8Str.size() == 1) {
                                    text += utf8Str;
                                    pos += 2;
                                    maybeUtf8 = true;
                                }
                            }
                        }

                        if (!maybeUtf8) {
                            // Fallback: treat as Latin-1
                            text += QString::fromLatin1(raw);
                        }
                    }
                }
                pos++;
                break;
        }
    }

    // Clean up: collapse multiple newlines
    static QRegularExpression multiNewline("\n{3,}");
    text.replace(multiNewline, "\n\n");

    return text.trimmed();
}

RtfParser::Result RtfParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // Read the file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        result.errorMessage = "Failed to open RTF file: " + file.errorString();
        return result;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.isEmpty()) {
        result.errorMessage = "RTF file is empty";
        return result;
    }

    // Verify it starts with {\rtf
    if (!data.startsWith("{\\rtf") && !data.startsWith("{\\RTF")) {
        result.errorMessage = "File does not appear to be valid RTF";
        return result;
    }

    // Extract text
    result.text = extractRtfText(data);
    result.metadata = extractMetadata(filePath);
    result.metadata["mimeType"] = "application/rtf";
    result.success = true;

    return result;
}

QMap<QString, QString> RtfParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = "rtf";
    meta["mimeType"] = "application/rtf";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}
