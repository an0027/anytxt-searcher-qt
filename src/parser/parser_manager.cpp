/*
 * parser_manager.cpp - 解析器管理器实现

实现解析器的注册、分发和文件处理流程。
条件编译：支持 HAS_POPPLER, HAS_TESSERACT, HAS_LIBZIP 控制。
 */

#include "parser/parser_manager.h"
#include "parser/text_parser.h"
#include "parser/eml_parser.h"
#include "parser/rtf_parser.h"
#include <QFileInfo>
#include <QDebug>

#ifdef HAS_POPPLER
#include "parser/pdf_parser.h"
#endif

#ifdef HAS_LIBZIP
#include "parser/docx_parser.h"
#include "parser/xlsx_parser.h"
#include "parser/pptx_parser.h"
#include "parser/epub_parser.h"
#include "parser/doc_parser.h"
#include "parser/wps_parser.h"
#endif

#ifdef HAS_TESSERACT
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>
#endif

ParserManager::ParserManager()
{
    // Register default processors
    registerProcessor(std::make_shared<TextParser>());

#ifdef HAS_POPPLER
    registerProcessor(std::make_shared<PdfParser>());
#else
    qDebug() << "PdfParser disabled (HAS_POPPLER not defined)";
#endif

#ifdef HAS_LIBZIP
    registerProcessor(std::make_shared<DocxParser>());
    registerProcessor(std::make_shared<XlsxParser>());
    registerProcessor(std::make_shared<PptxParser>());
    registerProcessor(std::make_shared<EpubParser>());
    registerProcessor(std::make_shared<DocParser>());
    registerProcessor(std::make_shared<WpsParser>());
#else
    qDebug() << "DocxParser disabled (HAS_LIBZIP not defined)";
#endif
}

ParserManager::~ParserManager()
{
    m_processors.clear();
    m_extensionMap.clear();
}

void ParserManager::registerProcessor(std::shared_ptr<DocumentProcessor> processor)
{
    QStringList extensions = processor->supportedExtensions();

    // Register all supported extensions for this processor
    for (const auto& ext : extensions) {
        m_extensionMap[ext.toLower()] = processor;
    }

    m_processors.push_back(std::move(processor));
}

DocumentProcessor* ParserManager::getProcessor(const QString& filePath) const
{
    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();

    auto it = m_extensionMap.find(ext);
    if (it != m_extensionMap.end()) {
        return it.value().get();
    }

    // Try to find by MIME type matching
    for (const auto& proc : m_processors) {
        if (proc->canProcess(filePath)) {
            return proc.get();
        }
    }

    return nullptr;
}

QStringList ParserManager::supportedExtensions() const
{
    return m_extensionMap.keys();
}

DocumentProcessor::Result ParserManager::processDocument(const QString& filePath)
{
    QFileInfo fi(filePath);
    if (!fi.exists() || !fi.isReadable()) {
        DocumentProcessor::Result result;
        result.success = false;
        result.errorMessage = "File does not exist or is not readable: " + filePath;
        return result;
    }

    QString ext = fi.suffix().toLower();

    // Check for image files - use Tesseract OCR
    QStringList imageExts = {"png", "jpg", "jpeg", "gif", "bmp", "tiff", "tif", "webp", "pnm"};
    if (imageExts.contains(ext)) {
#ifdef HAS_TESSERACT
        DocumentProcessor::Result result;
        result.text = ocrImage(filePath);
        result.success = !result.text.isEmpty();
        result.metadata["fileSize"] = QString::number(fi.size());
        result.metadata["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
        result.metadata["fileExt"] = ext;
        result.metadata["mimeType"] = "image/" + ext;
        result.metadata["fileName"] = fi.fileName();
        result.metadata["filePath"] = fi.absoluteFilePath();
        if (!result.success) {
            result.errorMessage = "OCR failed for image: " + filePath;
        }
        return result;
#else
        DocumentProcessor::Result result;
        result.success = false;
        result.errorMessage = "OCR disabled (requires tesseract/leptonica): " + filePath;
        return result;
#endif
    }

    DocumentProcessor* processor = getProcessor(filePath);
    if (!processor) {
        DocumentProcessor::Result result;
        result.success = false;
        result.errorMessage = "No processor found for file: " + filePath;
        qDebug() << "No processor for:" << filePath << "(ext:" << ext << ")";
        return result;
    }

    qDebug() << "Processing with" << processor->name() << ":" << filePath;
    return processor->extractText(filePath);
}

#ifdef HAS_TESSERACT
QString ParserManager::ocrImage(const QString& imagePath)
{
    try {
        tesseract::TessBaseAPI api;
        if (api.Init(nullptr, "eng+chi_sim")) {
            qWarning() << "Failed to initialize Tesseract OCR";
            return {};
        }

        // Read the image using Leptonica
        Pix* pix = pixRead(imagePath.toUtf8().constData());
        if (!pix) {
            qWarning() << "Failed to read image for OCR:" << imagePath;
            return {};
        }

        api.SetImage(pix);
        char* text = api.GetUTF8Text();
        QString result = QString::fromUtf8(text);

        delete[] text;
        pixDestroy(&pix);
        api.End();

        return result.trimmed();
    } catch (const std::exception& e) {
        qWarning() << "OCR error:" << e.what();
        return {};
    }
}
#else
QString ParserManager::ocrImage(const QString& /*imagePath*/)
{
    qWarning() << "OCR unavailable (HAS_TESSERACT not defined)";
    return {};
}
#endif
