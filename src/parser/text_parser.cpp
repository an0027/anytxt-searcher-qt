/*
 * text_parser.cpp - 纯文本文件解析器实现 (Qt6 native, no Core5Compat)
 */

#include <QDateTime>
#include <algorithm>
#include "parser/text_parser.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QStringDecoder>

TextParser::TextParser() {}

bool TextParser::canProcess(const QString& filePath) const
{
    QStringList exts = supportedExtensions();
    QFileInfo fi(filePath);
    return exts.contains(fi.suffix().toLower());
}

QStringList TextParser::supportedExtensions() const
{
    return {"txt", "text", "md", "markdown", "csv", "tsv", "log",
            "ini", "cfg", "conf", "xml", "json", "yaml", "yml",
            "toml", "cfg", "properties", "env", "sh", "bash", "zsh",
            "py", "js", "ts", "jsx", "tsx", "c", "cpp", "h", "hpp",
            "java", "kt", "scala", "rb", "pl", "php", "rs", "go",
            "css", "scss", "less", "sql", "r", "m", "swift",
            "bat", "ps1", "lua", "html", "htm", "xhtml",
            "cmake", "makefile", "dockerfile", "gitignore"};
}

QStringList TextParser::supportedMimeTypes() const
{
    return {
        "text/plain", "text/markdown", "text/csv", "text/xml",
        "text/html", "application/json", "application/xml"
    };
}

QByteArray TextParser::readFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << filePath << file.errorString();
        return {};
    }
    QByteArray data = file.readAll();
    file.close();
    return data;
}

QString TextParser::detectEncoding(const QByteArray& data) const
{
    // BOM detection
    if (data.startsWith(QByteArray("\xEF\xBB\xBF"))) return "UTF-8";
    if (data.startsWith(QByteArray("\xFF\xFE"))) return "UTF-16LE";
    if (data.startsWith(QByteArray("\xFE\xFF"))) return "UTF-16BE";

    // Check if valid UTF-8
    QString utf8Test = QString::fromUtf8(data);
    QByteArray reencoded = utf8Test.toUtf8();
    if (reencoded == data) return "UTF-8";

    // Try system locale encoding
    auto sysDecoder = QStringDecoder(QStringConverter::System);
    if (sysDecoder.isValid()) {
        QString sysTest = sysDecoder(data);
        auto sysEncoder = QStringEncoder(QStringConverter::System);
        if (sysEncoder(sysTest) == data)
            return "System";
    }

    return "Latin-1";
}

QString TextParser::decodeText(const QByteArray& data, const QString& encoding) const
{
    if (data.isEmpty()) return {};

    if (encoding == "UTF-8" || encoding.startsWith("UTF-8"))
        return QString::fromUtf8(data);
    if (encoding == "Latin-1")
        return QString::fromLatin1(data);
    if (encoding == "System") {
        auto dec = QStringDecoder(QStringConverter::System);
        if (dec.isValid()) return dec(data);
        return QString::fromUtf8(data);
    }
    return QString::fromUtf8(data);
}

TextParser::Result TextParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    QByteArray data = readFile(filePath);
    if (data.isEmpty()) {
        result.errorMessage = "File is empty or cannot be read";
        return result;
    }

    QString encoding = detectEncoding(data);
    result.text = decodeText(data, encoding);

    if (result.text.startsWith(QChar(0xFEFF)))
        result.text = result.text.mid(1);

    // Remove empty lines preserving paragraph structure
    QStringList lines = result.text.split('\n');
    lines.erase(std::remove_if(lines.begin(), lines.end(),
        [](const QString& line) { return line.trimmed().isEmpty(); }), lines.end());
    result.text = lines.join('\n');

    result.metadata = extractMetadata(filePath);
    result.metadata["encoding"] = encoding;
    result.success = true;
    return result;
}

QMap<QString, QString> TextParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);
    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = fi.suffix().toLower();
    meta["mimeType"] = "text/plain";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();
    return meta;
}
