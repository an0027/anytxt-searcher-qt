// Realistic benchmark for AnyTXT Searcher C++ version
// Tests bulk indexing with batch commits (like real usage)
#include <xapian.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QTextStream>
#include <QRandomGenerator>
#include <QStringList>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);

    int numFiles = 1000;
    int numSearches = 100;
    int batchSize = 500;

    for (int i = 1; i < argc; i++) {
        QString arg(argv[i]);
        if (arg.startsWith("--files=")) numFiles = arg.mid(8).toInt();
        if (arg.startsWith("--searches=")) numSearches = arg.mid(11).toInt();
        if (arg.startsWith("--batch=")) batchSize = arg.mid(8).toInt();
    }

    qDebug() << "\n=== C++ Performance Benchmark ===";
    qDebug() << "Files:" << numFiles << "| Searches:" << numSearches << "| Batch:" << batchSize;

    QString testDir = "/tmp/anytxt_benchmark";
    QDir().mkpath(testDir);
    QString indexDir = testDir + "/index";
    QString docDir = testDir + "/docs";
    QDir().mkpath(docDir);

    qDebug() << "\n--- Generating" << numFiles << "documents ---";
    QElapsedTimer genTimer; genTimer.start();

    QStringList keywords = {
        "人工智能", "机器学习", "深度学习", "自然语言处理", "计算机视觉",
        "数据挖掘", "大数据", "云计算", "区块链", "物联网",
        "Xapian", "Qt", "C++", "Python", "性能优化",
        "AnyTXT", "全文搜索", "索引", "检索", "文档分析"
    };

    int64_t totalChars = 0;
    for (int i = 0; i < numFiles; i++) {
        QString fn = QString("doc_%1.txt").arg(i, 6, 10, QChar('0'));
        QFile file(docDir + "/" + fn);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << "文档编号: " << i << "\n标题: 测试文档 " << i << "\n\n";
            int np = QRandomGenerator::global()->bounded(3, 8);
            for (int p = 0; p < np; p++) {
                out << "这是第" << p << "段。";
                int nk = QRandomGenerator::global()->bounded(1, 4);
                for (int k = 0; k < nk; k++) {
                    out << keywords[QRandomGenerator::global()->bounded(keywords.size())]
                        << "是重要的技术方向。";
                }
                out << "\n";
            }
            file.close();
            totalChars += file.size();
        }
    }
    qint64 genTime = genTimer.elapsed();
    qDebug() << "Generation:" << genTime << "ms |" << totalChars << "bytes";

    // ===== INDEXING =====
    qDebug() << "\n--- Indexing" << numFiles << "documents ---";
    QElapsedTimer idxTimer; idxTimer.start();

    Xapian::WritableDatabase wdb(indexDir.toStdString(), Xapian::DB_CREATE_OR_OPEN);
    Xapian::Stem stemmer("english");
    Xapian::TermGenerator termgen;
    termgen.set_stemmer(stemmer);
    termgen.set_flags(Xapian::TermGenerator::FLAG_CJK_NGRAM);

    for (int i = 0; i < numFiles; i++) {
        QString fn = QString("doc_%1.txt").arg(i, 6, 10, QChar('0'));
        QString fp = docDir + "/" + fn;
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QString content = f.readAll();
        f.close();
        QFileInfo fi(fp);

        Xapian::Document doc;
        termgen.set_document(doc);
        termgen.index_text(content.toStdString());
        doc.add_value(0, QString("%1").arg(fi.lastModified().toSecsSinceEpoch(), 20, 10, QChar('0')).toStdString());
        doc.add_value(1, QString("%1").arg(fi.size(), 20, 10, QChar('0')).toStdString());
        doc.add_boolean_term("XEXTtxt");
        wdb.replace_document(("P" + fp.toStdString()), doc);

        if ((i + 1) % batchSize == 0 || i == numFiles - 1) {
            wdb.commit();
        }
    }
    wdb.close();

    qint64 idxTime = idxTimer.elapsed();
    double dps = numFiles * 1000.0 / (idxTime > 0 ? idxTime : 1);
    qDebug() << "Index time:" << idxTime << "ms";
    qDebug() << "Speed:" << dps << "docs/sec";

    // ===== SEARCH =====
    qDebug() << "\n--- Searching" << numSearches << "queries ---";
    Xapian::Database db(indexDir.toStdString());
    Xapian::QueryParser qp;
    qp.set_stemmer(stemmer);
    qp.set_stemming_strategy(Xapian::QueryParser::STEM_SOME);
    qp.set_database(db);
    unsigned flags = Xapian::QueryParser::FLAG_DEFAULT | Xapian::QueryParser::FLAG_CJK_NGRAM;

    QElapsedTimer srchTimer; srchTimer.start();
    int totalResults = 0;
    for (int i = 0; i < numSearches; i++) {
        QString q = keywords[i % keywords.size()];
        Xapian::Query query = qp.parse_query(q.toStdString(), flags);
        Xapian::Enquire enquire(db);
        enquire.set_query(query);
        Xapian::MSet mset = enquire.get_mset(0, 20);
        totalResults += mset.get_matches_estimated();
    }
    qint64 srchTime = srchTimer.elapsed();
    double avgMs = srchTime * 1.0 / numSearches;
    double qps = numSearches * 1000.0 / (srchTime > 0 ? srchTime : 1);
    qDebug() << "Search time:" << srchTime << "ms";
    qDebug() << "Avg search:" << avgMs << "ms";
    qDebug() << "QPS:" << qps;
    qDebug() << "Matches:" << totalResults;

    // ===== DETAILED =====
    qDebug() << "\n--- Detailed search ---";
    {
        QElapsedTimer dt; dt.start();
        Xapian::Query q = qp.parse_query("人工智能", flags);
        Xapian::Enquire enq(db);
        enq.set_query(q);
        Xapian::MSet m = enq.get_mset(0, 5);
        qDebug() << "'人工智能'-> " << m.get_matches_estimated() << "matches in" << dt.elapsed() << "ms";
    }
    db.close();

    double estMin = (600000.0 / dps) / 60.0;
    qDebug() << "\n=== Summary ===";
    qDebug() << "Files:" << numFiles << "| Data:" << totalChars << "bytes";
    qDebug() << "Index:" << idxTime << "ms";
    qDebug() << "Speed:" << dps << "docs/sec";
    qDebug() << "Avg search:" << avgMs << "ms";
    qDebug() << "Est 600K index:" << QString::number(estMin, 'f', 1) << "min";

    QDir(indexDir).removeRecursively();
    QDir(docDir).removeRecursively();
    QDir(testDir).rmdir(".");
    return 0;
}
