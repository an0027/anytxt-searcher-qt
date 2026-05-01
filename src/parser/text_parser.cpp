/*
 * text_parser.cpp - 纯文本文件解析器实现

实现文本文件的读取、编码检测和内容提取。
 */

#include <QDateTime>
#include <algorithm>
#include "parser/text_parser.h"
#include <QFile>
#include <QFileInfo>
#include <QDebug>

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtCore5Compat/QTextCodec>
#else
#include <QTextCodec>
#endif

TextParser::TextParser()
{
}

bool TextParser::canProcess(const QString& filePath) const
{
    QStringList exts = supportedExtensions();
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    return exts.contains(ext);
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
        "text/html", "application/json", "application/xml",
        "text/x-python", "text/x-java", "text/x-c",
        "text/x-script.sh", "text/x-ruby", "text/x-php"
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
    // Check for BOM first
    if (data.startsWith(QByteArray("\xEF\xBB\xBF"))) {
        return "UTF-8";
    }
    if (data.startsWith(QByteArray("\xFF\xFE"))) {
        return "UTF-16LE";
    }
    if (data.startsWith(QByteArray("\xFE\xFF"))) {
        return "UTF-16BE";
    }

    // Check if valid UTF-8 (most common)
    QString utf8Test = QString::fromUtf8(data);
    QByteArray reencoded = utf8Test.toUtf8();
    if (reencoded == data) {
        return "UTF-8";
    }

    // Try GBK/GB2312 for Chinese text
    QTextCodec* gbkCodec = QTextCodec::codecForName("GBK");
    if (gbkCodec) {
        QString gbkTest = gbkCodec->toUnicode(data);
        QByteArray gbkReencoded = gbkCodec->fromUnicode(gbkTest);
        if (gbkReencoded == data) {
            return "GBK";
        }
    }

    // Try system locale encoding
    QTextCodec* localeCodec = QTextCodec::codecForLocale();
    if (localeCodec) {
        QString localeTest = localeCodec->toUnicode(data);
        QByteArray localeReencoded = localeCodec->fromUnicode(localeTest);
        if (localeReencoded == data) {
            return localeCodec->name();
        }
    }

    // Try Latin-1 (ISO 8859-1) as fallback; this always "succeeds"
    return "ISO 8859-1";
}

QString TextParser::decodeText(const QByteArray& data, const QString& encoding) const
{
    if (data.isEmpty()) return {};

    QTextCodec* codec = QTextCodec::codecForName(encoding.toLatin1());
    if (!codec) {
        qWarning() << "Unsupported encoding:" << encoding << ", falling back to UTF-8";
        codec = QTextCodec::codecForName("UTF-8");
        if (!codec) {
            return QString::fromUtf8(data);
        }
    }

    return codec->toUnicode(data);
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

    // Remove BOM if present
    if (result.text.startsWith(QChar(0xFEFF))) {
        result.text = result.text.mid(1);
    }

    // Remove empty lines (preserve paragraph structure without gaps)
    QStringList lines = result.text.split('\n');
    lines.erase(std::remove_if(lines.begin(), lines.end(),
        [](const QString& line) { return line.trimmed().isEmpty(); }), lines.end());
    result.text = lines.join('\n');

    // Extract metadata
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
