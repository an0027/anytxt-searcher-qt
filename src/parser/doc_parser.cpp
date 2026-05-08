/*
 * doc_parser.cpp - 鑰佺増 Word .doc 鏂囦欢瑙ｆ瀽鍣ㄥ疄鐜? *
 * 瀹炵幇 .doc 鏍煎紡鐨?Word 97-2003 鏂囨。鏂囨湰鎻愬彇銆? * 瑙ｆ瀽 OLE2 澶嶅悎鏂囨。瀹瑰櫒锛岃鍙?WordDocument stream锛? * 浠?FIB (File Information Block) 涓彁鍙栨枃鏈€? *
 * 鏉′欢缂栬瘧锛氫粎鍦?HAS_LIBZIP 瀹氫箟鏃舵彁渚涘畬鏁村疄鐜般€? * 娉ㄦ剰锛氭湰瀹炵幇浣跨敤绾?C++ 瑙ｆ瀽 OLE2 缁撴瀯锛屼笉渚濊禆澶栭儴 OLE 搴撱€? */

#include <QDateTime>
#include "parser/doc_parser.h"
#include <QFileInfo>
#include <QFile>
#include <QDebug>
#include <QStringDecoder>
#include <QDataStream>
#include <QtEndian>
#include <cstring>

#ifdef HAS_LIBZIP

// OLE2 magic bytes
static const quint8 OLE2_MAGIC[] = {0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1};
// FIB magic for Word doc
static const quint16 FIB_MAGIC = 0xA5EC;

// Directory entry type constants
static const quint8 STGTY_INVALID = 0;
static const quint8 STGTY_STORAGE = 1;
static const quint8 STGTY_STREAM = 2;
static const quint8 STGTY_LOCKBYTES = 3;
static const quint8 STGTY_PROPERTY = 4;
static const quint8 STGTY_ROOT = 5;

// Directory entry name length offset
static const int MAX_NAME_LEN = 64;

DocParser::DocParser()
{
}

bool DocParser::canProcess(const QString& filePath) const
{
    QFileInfo fi(filePath);
    return fi.suffix().toLower() == "doc";
}

QStringList DocParser::supportedExtensions() const
{
    return {"doc"};
}

QStringList DocParser::supportedMimeTypes() const
{
    return {"application/msword"};
}

QByteArray DocParser::readFileBytes(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open .doc file:" << filePath;
        return {};
    }
    return file.readAll();
}

qint32 DocParser::getSectorSize(const Ole2Header* hdr) const
{
    return 1 << hdr->sectorPower;
}

qint32 DocParser::getMiniSectorSize(const Ole2Header* hdr) const
{
    return 1 << hdr->miniSectorPower;
}

quint32 DocParser::readUint32(const QByteArray& data, int offset) const
{
    if (offset + 4 > data.size()) return 0xFFFFFFFF;
    // OLE2 uses little-endian
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

quint16 DocParser::readUint16(const QByteArray& data, int offset) const
{
    if (offset + 2 > data.size()) return 0xFFFF;
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(data.constData() + offset));
}

QString DocParser::readWordDocumentStream(const QByteArray& oleData) const
{
    if (oleData.size() < static_cast<int>(sizeof(Ole2Header))) {
        qWarning() << "DOC: OLE2 data too small for header";
        return {};
    }

    const Ole2Header* hdr = reinterpret_cast<const Ole2Header*>(oleData.constData());
    qint32 sectorSize = getSectorSize(hdr);
    qint32 miniSectorSize = getMiniSectorSize(hdr);

    if (sectorSize <= 0 || miniSectorSize <= 0) {
        qWarning() << "DOC: Invalid sector sizes";
        return {};
    }

    // Read FAT
    QVector<quint32> fat;
    for (int i = 0; i < 109; ++i) {
        quint32 sector = qFromLittleEndian<quint32>(hdr->DIFAT[i]);
        if (sector == 0xFFFFFFFF || sector == 0) break; // End of FAT chain
        fat.append(sector);
    }

    // Also follow additional DIFAT sectors
    if (hdr->firstDIFATSector != 0xFFFFFFFE && hdr->numDIFATSectors > 0) {
        quint32 difatSector = hdr->firstDIFATSector;
        for (quint32 d = 0; d < hdr->numDIFATSectors && difatSector != 0xFFFFFFFE; ++d) {
            int offset = static_cast<int>(difatSector) * sectorSize + sectorSize; // +sectorSize for header
            if (offset + sectorSize > oleData.size()) break;

            const uchar* sectorData = reinterpret_cast<const uchar*>(oleData.constData() + offset);
            int entriesPerSector = sectorSize / 4 - 1; // Last 4 bytes point to next DIFAT
            for (int i = 0; i < entriesPerSector; ++i) {
                quint32 nextSector = qFromLittleEndian<quint32>(sectorData + i * 4);
                if (nextSector == 0xFFFFFFFF) break;
                fat.append(nextSector);
            }
            // Read next DIFAT sector
            difatSector = qFromLittleEndian<quint32>(sectorData + entriesPerSector * 4);
        }
    }

    // Read directory entries to find the "WordDocument" stream
    quint32 dirSector = hdr->firstDirSector;
    QByteArray wordDocData;
    QByteArray tableData;

    // Directory entry size is 128 bytes, entries per sector is sectorSize / 128
    int entriesPerSector = sectorSize / 128;

    while (dirSector != 0xFFFFFFFE) {
        int dirOffset = static_cast<int>(dirSector) * sectorSize + sectorSize;
        if (dirOffset + sectorSize > oleData.size()) break;

        for (int e = 0; e < entriesPerSector; ++e) {
            int entryOffset = dirOffset + e * 128;
            if (entryOffset + 128 > oleData.size()) break;

            // Read directory entry name (UTF-16LE, up to 32 characters = 64 bytes)
            quint16 nameSize = qFromLittleEndian<quint16>(oleData.constData() + entryOffset + 64);
            if (nameSize > 64) nameSize = 64;

            QString entryName;
            if (nameSize > 0) {
                QByteArray nameBytes(oleData.constData() + entryOffset, nameSize);
                entryName = QString::fromUtf16(reinterpret_cast<const char16_t*>(nameBytes.constData()), nameSize / 2);
                // Remove null terminator if present
                int nullPos = entryName.indexOf(QChar(0));
                if (nullPos >= 0) entryName = entryName.left(nullPos);
            }

            if (entryName == "WordDocument") {
                // Read the stream
                quint32 startSector = qFromLittleEndian<quint32>(oleData.constData() + entryOffset + 116);
                quint32 streamSize = qFromLittleEndian<quint64>(oleData.constData() + entryOffset + 120);

                if (streamSize > static_cast<quint32>(oleData.size())) {
                    qWarning() << "DOC: WordDocument stream size too large:" << streamSize;
                    continue;
                }

                // For small streams (< 4096 bytes), data is in the mini stream
                // For larger streams, use regular FAT
                if (streamSize < hdr->minSizeStandardStream) {
                    qDebug() << "DOC: WordDocument is a mini stream";
                    // TODO: Read from mini stream
                    // For now, try to read directly using FAT
                }

                // Follow the FAT chain for this stream
                quint32 currentSector = startSector;
                int maxSectors = 10000; // Safety limit
                while (currentSector != 0xFFFFFFFE && currentSector != 0xFFFFFFFF && maxSectors-- > 0) {
                    int sectorOffset = static_cast<int>(currentSector) * sectorSize + sectorSize;
                    if (sectorOffset + sectorSize > oleData.size()) break;

                    // Determine how much to read (last sector might be partial)
                    int bytesToRead = qMin(sectorSize, static_cast<int>(streamSize) - wordDocData.size());
                    if (bytesToRead <= 0) break;

                    wordDocData.append(oleData.constData() + sectorOffset, bytesToRead);

                    if (static_cast<quint32>(currentSector) >= static_cast<quint32>(fat.size())) break;
                    currentSector = fat[currentSector];
                }
            } else if (entryName == "1Table" || entryName == "0Table") {
                // Table stream contains additional data
                quint32 startSector = qFromLittleEndian<quint32>(oleData.constData() + entryOffset + 116);
                quint32 streamSize = qFromLittleEndian<quint64>(oleData.constData() + entryOffset + 120);

                quint32 currentSector = startSector;
                int maxSectors = 10000;
                while (currentSector != 0xFFFFFFFE && currentSector != 0xFFFFFFFF && maxSectors-- > 0) {
                    int sectorOffset = static_cast<int>(currentSector) * sectorSize + sectorSize;
                    if (sectorOffset + sectorSize > oleData.size()) break;

                    int bytesToRead = qMin(sectorSize, static_cast<int>(streamSize) - tableData.size());
                    if (bytesToRead <= 0) break;

                    tableData.append(oleData.constData() + sectorOffset, bytesToRead);

                    if (static_cast<quint32>(currentSector) >= static_cast<quint32>(fat.size())) break;
                    currentSector = fat[currentSector];
                }
            }
        }

        // Move to next directory sector
        if (static_cast<quint32>(dirSector) >= static_cast<quint32>(fat.size())) break;
        dirSector = fat[dirSector];
    }

    if (wordDocData.isEmpty()) {
        qWarning() << "DOC: WordDocument stream not found";
        return {};
    }

    return QString::fromUtf8(wordDocData);
}

QString DocParser::extractDocText(const QByteArray& oleData) const
{
    // Read the WordDocument stream
    QString rawDocData = readWordDocumentStream(oleData);
    if (rawDocData.isEmpty()) {
        return {};
    }

    QByteArray docData = rawDocData.toUtf8();

    if (docData.size() < 512) {
        qWarning() << "DOC: WordDocument stream too small";
        return {};
    }

    // Parse FIB (File Information Block) at the beginning of WordDocument stream
    // FIB starts at byte 0
    // wIdent at offset 0 (2 bytes)
    quint16 wIdent = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(docData.constData()));
    if (wIdent != FIB_MAGIC) {
        qWarning() << "DOC: Invalid FIB magic:" << QString::number(wIdent, 16);
    }

    // FIB structure (simplified):
    // Offset 0:  wIdent (2 bytes) = 0xA5EC
    // Offset 2:  nFib (2 bytes) - file version
    // Offset 4:  Product (2 bytes)
    // Offset 6:  lidFE (2 bytes) - language
    // Offset 8:  pnNext (2 bytes)
    // Offset 10: flags (2 bytes)
    // ...
    // For Word 97+, the text is stored in the Complex section
    // ccpText at FIB offset 0x004C (76)
    // ccpFtn at FIB offset 0x0050 (80)
    // ...
    // fcClx at FIB offset 0x01A2 (418) in Word 2000+
    // lcbClx at FIB offset 0x01A6 (422)

    // For a simpler approach, try to extract text directly from the WordDocument stream
    // The text in simple Word docs is stored as:
    // 1. Piece table (in the WordDocument stream after the FIB)
    // 2. The actual text follows

    // Simplified extraction: look for text after the FIB
    // The FIB is variable-length, but typically 32-80 bytes for simple docs
    // For Word 97 docs, FIB base is 32 bytes, plus FibRgW, FibRgLw, FibRgFcLcbBlob

    // Try to find text by looking for ASCII-like content in the stream
    // The text in a .doc file is stored as complex-script-aware pieces

    // More robust: use the Clx (Complex part) to find text
    // For Word 97 FIB, fcClx and lcbClx are at different offsets depending on nFib

    // For a basic implementation, try to find raw text by looking at
    // the characters after the FIB header area

    // Check if nFib >= 193 (Word 97) or nFib >= 192 (Word 95)
    quint16 nFib = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(docData.constData() + 2));

    // For a minimal but working implementation:
    // 1. Skip FIB (roughly 512 bytes)
    // 2. Look for Unicode or ANSI text content

    // The piece table in Word doc: find the Clx in the WordDocument stream
    // Clx starts with a byte that tells us the encoding

    // Simpler approach: scan the WordDocument stream for continuous text
    QString result;
    int startOffset = 512; // FIB is at most one sector

    // Try to find text in two passes:
    // Pass 1: look for Unicode text (2-byte chars with null in between)
    bool foundUnicode = false;
    int unicodeRunStart = -1;
    int unicodeRunLen = 0;
    int bestUnicodeStart = -1;
    int bestUnicodeLen = 0;

    for (int i = startOffset; i < docData.size() - 1; i += 2) {
        quint8 hi = static_cast<quint8>(docData[i + 1]);
        quint8 lo = static_cast<quint8>(docData[i]);

        // Check if this looks like ASCII in UTF-16LE: lo is ASCII, hi is 0
        if (hi == 0 && lo >= 0x20 && lo <= 0x7E) {
            if (unicodeRunStart < 0) unicodeRunStart = i;
            unicodeRunLen++;
        } else if (lo == 0 && hi >= 0x20 && hi <= 0x7E) {
            // UTF-16BE? Unlikely for .doc but handle gracefully
            // Check if lo is 0 and hi is ASCII (swapped endianness)
        } else {
            if (unicodeRunLen > bestUnicodeLen) {
                bestUnicodeLen = unicodeRunLen;
                bestUnicodeStart = unicodeRunStart;
            }
            unicodeRunStart = -1;
            unicodeRunLen = 0;
        }
    }

    if (bestUnicodeLen > 3) {
        foundUnicode = true;
        // Extract Unicode text
        for (int i = bestUnicodeStart; i < docData.size() - 1 && i < bestUnicodeStart + bestUnicodeLen * 2; i += 2) {
            quint8 lo = static_cast<quint8>(docData[i]);
            result += QChar(lo);
        }
    }

    // Pass 2: look for ANSI text if Unicode text not found or too short
    if (!foundUnicode || result.length() < 20) {
        result.clear();
        int ansiRunStart = -1;
        int ansiRunLen = 0;
        int bestAnsiStart = -1;
        int bestAnsiLen = 0;

        for (int i = startOffset; i < docData.size(); ++i) {
            quint8 ch = static_cast<quint8>(docData[i]);
            if (ch >= 0x20 && ch <= 0x7E) {
                if (ansiRunStart < 0) ansiRunStart = i;
                ansiRunLen++;
            } else if (ch == 0x0D || ch == 0x0A) {
                // Carriage return or newline - keep as part of text
                if (ansiRunStart >= 0) ansiRunLen++;
            } else {
                if (ansiRunLen > bestAnsiLen) {
                    bestAnsiLen = ansiRunLen;
                    bestAnsiStart = ansiRunStart;
                }
                ansiRunStart = -1;
                ansiRunLen = 0;
            }
        }

        if (bestAnsiLen > 3) {
            for (int i = bestAnsiStart; i < docData.size() && i < bestAnsiStart + bestAnsiLen; ++i) {
                quint8 ch = static_cast<quint8>(docData[i]);
                if (ch == 0x0D) {
                    result += '\n';
                } else if (ch >= 0x20 && ch <= 0x7E) {
                    result += QChar(ch);
                }
            }
        }
    }

    return result.trimmed();
}

DocParser::Result DocParser::extractText(const QString& filePath)
{
    Result result;
    result.success = false;

    // Read the entire file as OLE2 compound document
    QByteArray oleData = readFileBytes(filePath);
    if (oleData.isEmpty()) {
        result.errorMessage = "Failed to read .doc file";
        return result;
    }

    // Verify OLE2 magic
    if (oleData.size() < 8 || memcmp(oleData.constData(), OLE2_MAGIC, 8) != 0) {
        result.errorMessage = "File is not a valid OLE2 compound document (.doc)";
        return result;
    }

    // Extract text
    result.text = extractDocText(oleData);
    result.metadata = extractMetadata(filePath);
    result.metadata["mimeType"] = "application/msword";

    if (result.text.isEmpty()) {
        result.errorMessage = "Failed to extract text from .doc file (may be encrypted or complex format)";
        qWarning() << "DOC: No text extracted from" << filePath;
        // Still mark as success if we got metadata
        result.success = false;
    } else {
        result.success = true;
    }

    return result;
}

QMap<QString, QString> DocParser::extractMetadata(const QString& filePath)
{
    QMap<QString, QString> meta;
    QFileInfo fi(filePath);

    meta["fileSize"] = QString::number(fi.size());
    meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
    meta["fileExt"] = "doc";
    meta["mimeType"] = "application/msword";
    meta["fileName"] = fi.fileName();
    meta["filePath"] = fi.absoluteFilePath();

    return meta;
}

#endif // HAS_LIBZIP

