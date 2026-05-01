// Must include xapian before Qt to avoid keyword clashes
#include <xapian.h>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QTextStream>
#include <QRandomGenerator>
#include <QCoreApplication>
#include <QStringList>
#include <iostream>

#include "core/config.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "core/xapian_searcher.h"
#include "core/document.h"

// Performance benchmark for AnyTXT Searcher C++ version
int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    int numFiles = 100;     // Default, can set with --files=N
    int numSearches = 50;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        QString arg(argv[i]);
        if (arg.startsWith("--files=")) {
            numFiles = arg.mid(8).toInt();
        }
        if (arg.startsWith("--searches=")) {
            numSearches = arg.mid(11).toInt();
        }
    }

    qDebug() << "=== AnyTXT Searcher C++ Performance Benchmark ===";
    qDebug() << "Test files:" << numFiles << "| Search iterations:" << numSearches;

    // Create temp directory
    QString testDir = "/tmp/anytxt_benchmark";
    QDir().mkpath(testDir);

    QString indexDir = testDir + "/index";
    QString docDir = testDir + "/docs";
    QDir().mkpath(docDir);

    // Generate test documents
    qDebug() << "\n--- Generating" << numFiles << "test documents ---";
    QElapsedTimer genTimer;
    genTimer.start();

    QStringList keywords = {
        "人工智能", "机器学习", "深度学习", "自然语言处理", "计算机视觉",
        "数据挖掘", "大数据", "云计算", "区块链", "物联网",
        "Xapian", "Qt", "C++", "Python", "性能优化",
        "AnyTXT", "全文搜索", "索引", "检索", "文档分析"
    };

    for (int i = 0; i < numFiles; i++) {
        QString fileName = QString("doc_%1.txt").arg(i, 6, 10, QChar('0'));
        QFile file(docDir + "/" + fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "文档编号: " << i << "\n";
            out << "标题: 测试文档 " << i << "\n\n";

            // Add random content with some keywords
            int numParagraphs = QRandomGenerator::global()->bounded(3, 10);
            for (int p = 0; p < numParagraphs; p++) {
                out << "这是第" << p << "段。";
                // Insert some keywords
                int numKeywords = QRandomGenerator::global()->bounded(1, 4);
                for (int k = 0; k < numKeywords; k++) {
                    int ki = QRandomGenerator::global()->bounded(keywords.size());
                    out << keywords[ki] << "是重要的技术方向。";
                }
                out << "\n";
            }
            file.close();
        }
    }

    qint64 genTime = genTimer.elapsed();
    qDebug() << "Document generation:" << genTime << "ms";

    // Initialize Xapian engine
    qDebug() << "\n--- Initializing Xapian engine ---";
    auto db = std::make_shared<XapianDatabase>();
    db->create(indexDir);

    IndexConfig config;
    config.dbPath = indexDir;

    auto indexer = std::make_shared<XapianIndexer>(db);
    auto searcher = std::make_shared<XapianSearcher>(db);

    // Benchmark indexing
    qDebug() << "\n--- Indexing" << numFiles << "documents ---";
    QElapsedTimer indexTimer;
    indexTimer.start();

    int indexed = 0;
    QDir docDir_(docDir);
    QStringList files = docDir_.entryList({"*.txt"}, QDir::Files, QDir::Name);

    for (const auto& file : files) {
        QString filePath = docDir + "/" + file;
        QFile f(filePath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = f.readAll();
            f.close();

            QMap<QString, QString> meta;
            QFileInfo fi(filePath);
            meta["fileSize"] = QString::number(fi.size());
            meta["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());
            meta["mimeType"] = "text/plain";
            meta["fileExt"] = "txt";

            try {
                indexer->addDocument(filePath, meta, content);
                indexed++;
            } catch (const std::exception& e) {
                qWarning() << "Index error for" << file << ":" << e.what();
            }
        }
    }

    qint64 indexTime = indexTimer.elapsed();
    double docsPerSec = numFiles * 1000.0 / (indexTime > 0 ? indexTime : 1);
    qDebug() << "Indexing time:" << indexTime << "ms";
    qDebug() << "Indexing speed:" << docsPerSec << "docs/sec";

    // Get document count
    int docCount = indexer->getDocumentCount();
    qint64 indexSize = indexer->getIndexSize();
    qDebug() << "Documents indexed:" << docCount;
    qDebug() << "Estimated index size:" << indexSize << "bytes";

    // Benchmark searching
    qDebug() << "\n--- Searching" << numSearches << "queries ---";
    QElapsedTimer searchTimer;
    searchTimer.start();

    int totalResults = 0;
    for (int i = 0; i < numSearches; i++) {
        QString query = keywords[i % keywords.size()];
        try {
            auto [results, totalHits] = searcher->search(query, 0, 10);
            totalResults += totalHits;
            Q_UNUSED(results);
        } catch (const std::exception& e) {
            qWarning() << "Search error:" << e.what();
        }
    }

    qint64 searchTime = searchTimer.elapsed();
    double searchesPerSec = numSearches * 1000.0 / (searchTime > 0 ? searchTime : 1);
    double avgSearchMs = searchTime * 1.0 / numSearches;
    qDebug() << "Search time:" << searchTime << "ms";
    qDebug() << "Average search:" << avgSearchMs << "ms";
    qDebug() << "Searches/sec:" << searchesPerSec;
    qDebug() << "Total results found:" << totalResults;

    // Benchmark a specific search
    qDebug() << "\n--- Detailed search test ---";
    try {
        QElapsedTimer detailTimer;
        detailTimer.start();
        auto [results, totalHits] = searcher->search("AnyTXT", 0, 50);
        qint64 detailTime = detailTimer.elapsed();

        qDebug() << "Query: 'AnyTXT' ->" << totalHits << "total matches";
        qDebug() << "Showing" << results.size() << "results";
        qDebug() << "Search time:" << detailTime << "ms";

        for (int i = 0; i < qMin(results.size(), 5); i++) {
            const auto& doc = results[i];
            qDebug() << "  [" << (i+1) << "]" << doc.fileName
                     << "- score:" << doc.percent << "%"
                     << "- path:" << doc.filePath;
        }
    } catch (const std::exception& e) {
        qWarning() << "Detailed search error:" << e.what();
    }

    // Cleanup
    db->close();
    QDir(indexDir).removeRecursively();
    QDir(docDir).removeRecursively();

    qDebug() << "\n=== Benchmark Summary ===";
    qDebug() << "Files:" << numFiles;
    qDebug() << "Index time:" << indexTime << "ms (" << docsPerSec << "docs/sec)";
    qDebug() << "Avg search:" << avgSearchMs << "ms";
    qDebug() << "Searches/sec:" << searchesPerSec;

    return 0;
}
