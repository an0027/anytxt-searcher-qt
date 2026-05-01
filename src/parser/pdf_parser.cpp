/*
 * pdf_parser.cpp - PDF 文件解析器实现

实现 PDF 文件的文本提取和元数据提取。
基于 poppler-cpp (无 Qt 依赖)。
条件编译：仅在 HAS_POPPLER 定义时提供完整实现。
 */

#include <QDateTime>
#include "parser/pdf_parser.h"
#include <QFileInfo>
#include <QDebug>
#include <memory>

#ifdef HAS_POPPLER

// Helper: convert poppler::ustring to QString
// to_utf8() returns byte_array = std::vector<char>
static inline QString ustr2qstr(const poppler::ustring &u)
{
    if (u.empty()) return {};
    auto utf8 = u.to_utf8();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

// Helper: convert time_t to QString
static inline QString time2qstr(time_t t)
{
    if (t == 0) return {};
    return QDateTime::fromSecsSinceEpoch(t).toString("yyyy-MM-dd HH:mm:ss");
}

PdfParser::PdfParser()
{
}

bool PdfParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == "pdf";
}

QStringList PdfParser::supportedExtensions() const
{
    return {"pdf"};
}

QStringList PdfParser::supportedMimeTypes() const
{
    return {"application/pdf"};
}

PdfParser::Result PdfParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // poppler-cpp load_from_file returns a raw pointer
    poppler::document *doc = poppler::document::load_from_file(filePath.toStdString());
    if (!doc) {
        result.errorMessage = "Failed to load PDF: " + filePath;
        qWarning() << result.errorMessage;
        return result;
    }

    // Wrap in unique_ptr for automatic cleanup
    std::unique_ptr<poppler::document> docPtr(doc);

    if (docPtr->is_locked()) {
        result.errorMessage = "PDF is password protected: " + filePath;
        qWarning() << result.errorMessage;
        return result;
    }

    QString fullText;
    int pageCount = docPtr->pages();

    for (int i = 0; i < pageCount; ++i) {
        // create_page returns a raw pointer
        poppler::page *pg = docPtr->create_page(i);
        if (!pg) {
            qWarning() << "Failed to load page" << i << "from" << filePath;
            continue;
        }

        std::unique_ptr<poppler::page> pagePtr(pg);
        poppler::ustring pageUstr = pagePtr->text();
        QString pageText = ustr2qstr(pageUstr);

        if (!pageText.isEmpty()) {
            if (i > 0) fullText += "\n\n";
            fullText += pageText;
        }

        // Limit total text size to prevent memory issues
        if (fullText.length() > 100000) {
            qDebug() << "PDF text truncated at 100K chars for" << filePath;
            break;
        }
    }

    result.text = fullText;
    result.metadata = extractMetadata(filePath);

    // Metadata from poppler document
    result.metadata["title"] = ustr2qstr(docPtr->get_title());
    result.metadata["author"] = ustr2qstr(docPtr->get_author());
    result.metadata["subject"] = ustr2qstr(docPtr->get_subject());
    result.metadata["keywords"] = ustr2qstr(docPtr->get_keywords());
    result.metadata["pageCount"] = QString::number(pageCount);
    {
        int major = 0, minor = 0;
        docPtr->get_pdf_version(&major, &minor);
        result.metadata["pdfVersion"] = QString("%1.%2").arg(major).arg(minor);
    }
    result.metadata["producer"] = ustr2qstr(docPtr->get_producer());
    result.metadata["creator"] = ustr2qstr(docPtr->get_creator());
    result.metadata["metadata"] = ustr2qstr(docPtr->metadata());

    time_t creationDate = docPtr->get_creation_date_t();
    if (creationDate > 0) {
        result.metadata["date"] = time2qstr(creationDate);
    }

    result.metadata["mimeType"] = "application/pdf";
    result.success = true;
    return result;
}

QMap<QString, QString> PdfParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = "pdf";
    meta["mimeType"] = "application/pdf";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}

#endif // HAS_POPPLER
