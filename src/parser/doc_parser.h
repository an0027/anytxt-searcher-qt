/*
 * doc_parser.h - 鑰佺増 Word .doc 鏂囦欢瑙ｆ瀽鍣? *
 * 鍔熻兘璇存槑锛氳В鏋?.doc 鏍煎紡鐨?Word 97-2003 鏂囨。锛? * 鍩轰簬 OLE2/Docfile 瀹瑰櫒璇诲彇 WordDocument stream銆? * 鏉′欢缂栬瘧锛氫粎鍦?HAS_LIBZIP 瀹氫箟鏃跺惎鐢ㄥ畬鏁村疄鐜? * 锛堝洜涓哄彲鑳界敤鍒?libzip 瑙?OLE 瀹瑰櫒锛夈€? */

#ifndef ANYTXT_DOC_PARSER_H
#define ANYTXT_DOC_PARSER_H

#include "core/document_processor.h"
#include <QByteArray>

#ifdef HAS_LIBZIP
#include <zip.h>

class DocParser : public DocumentProcessor {
public:
    DocParser();
    ~DocParser() override = default;

    QString name() const override { return "DocParser"; }
    Result extractText(const QString& filePath) override;
    QMap<QString, QString> extractMetadata(const QString& filePath) override;
    bool canProcess(const QString& filePath) const override;
    QStringList supportedExtensions() const override;
    QStringList supportedMimeTypes() const override;

private:
    struct OlsHeader {
        quint16 cbStd;      // size of this structure
        quint16 flags;
        // Followed by variable-length data
    };

    // OLE2 Compound Document header
    struct Ole2Header {
        quint8 magic[8];          // \xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1
        quint8 clsid[16];         // unused
        quint16 minorVersion;
        quint16 majorVersion;
        quint16 byteOrder;        // 0xFFFE
        quint16 sectorPower;      // usually 9 (512 bytes)
        quint16 miniSectorPower;  // usually 6 (64 bytes)
        quint16 reserved;
        quint32 reserved2;
        quint32 numDirectorySectors; // number of directory sectors (0 for small docs)
        quint32 numFATSectors;
        quint32 firstDirSector;
        quint32 minSizeStandardStream; // threshold for mini streams (usually 4096)
        quint32 miniStreamCutoff;      // same as above for some versions
        quint32 firstMiniFATSector;
        quint32 numMiniFATSectors;
        quint32 firstDIFATSector;
        quint32 numDIFATSectors;
        quint32 DIFAT[109];       // first 109 FAT sector locations
    };

    QByteArray readFileBytes(const QString& filePath) const;
    QString extractDocText(const QByteArray& oleData) const;
    QString readWordDocumentStream(const QByteArray& oleData) const;
    qint32 getSectorSize(const Ole2Header* hdr) const;
    qint32 getMiniSectorSize(const Ole2Header* hdr) const;
    quint32 readUint32(const QByteArray& data, int offset) const;
    quint16 readUint16(const QByteArray& data, int offset) const;
};

#else
// Stub: compile with HAS_LIBZIP to enable DOC parsing
class DocParser : public DocumentProcessor {
public:
    DocParser() = default;
    ~DocParser() override = default;

    QString name() const override { return "DocParser (unavailable)"; }
    Result extractText(const QString& /*filePath*/) override {
        Result r; r.success = false; r.errorMessage = "DOC parsing disabled (requires libzip)"; return r;
    }
    QMap<QString, QString> extractMetadata(const QString& /*filePath*/) override {
        return {};
    }
    bool canProcess(const QString& /*filePath*/) const override { return false; }
    QStringList supportedExtensions() const override { return {}; }
    QStringList supportedMimeTypes() const override { return {}; }
};
#endif

#endif // ANYTXT_DOC_PARSER_H

