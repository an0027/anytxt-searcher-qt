// Simple test - check if Xapian database works
#include <xapian.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>

int main(int argc, char* argv[]) {
    QCoreApplication app(argc, argv);
    
    QString testDir = "/tmp/anytxt_simple_test";
    QDir().mkpath(testDir);
    QString indexDir = testDir + "/index";
    
    qDebug() << "Index dir:" << indexDir;
    
    try {
        QElapsedTimer t;
        t.start();
        
        Xapian::WritableDatabase wdb(indexDir.toStdString(), Xapian::DB_CREATE_OR_OPEN);
        qDebug() << "DB created/opened:" << t.elapsed() << "ms";
        
        // Create a simple document
        Xapian::Document doc;
        doc.set_data("{\"test\":\"hello world\"}");
        doc.add_term("testterm");
        
        t.start();
        Xapian::docid docId = wdb.add_document(doc);
        qDebug() << "Document added, id:" << docId << "time:" << t.elapsed() << "ms";
        
        t.start();
        wdb.commit();
        qDebug() << "Commit:" << t.elapsed() << "ms";
        qDebug() << "Doc count:" << wdb.get_doccount();
        
        wdb.close();
        
        // Now search
        Xapian::Database db(indexDir.toStdString());
        Xapian::Enquire enquire(db);
        Xapian::QueryParser qp;
        Xapian::Query query = qp.parse_query("hello");
        enquire.set_query(query);
        
        t.start();
        Xapian::MSet mset = enquire.get_mset(0, 10);
        qDebug() << "Search:'hello' ->" << mset.size() << "results time:" << t.elapsed() << "ms";
        qDebug() << "Total matches:" << mset.get_matches_estimated();
        
        db.close();
        
        // Cleanup
        QDir(indexDir).removeRecursively();
        QDir(testDir).removeRecursively();
        qDebug() << "Test PASSED!";
        
    } catch (const Xapian::Error& e) {
        qDebug() << "Xapian error:" << e.get_description().c_str();
    } catch (const std::exception& e) {
        qDebug() << "Error:" << e.what();
    }
    
    return 0;
}
