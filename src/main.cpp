// ============================================================================
// main.cpp 鈥?AnyTXT Searcher 鍏ュ彛鏂囦欢
//
// 鍔熻兘璇存槑锛?
//   搴旂敤绋嬪簭鐨勪富鍏ュ彛鐐广€傚垵濮嬪寲 QApplication锛岃缃簲鐢ㄥ厓淇℃伅锛?
//   瑙ｆ瀽鍛戒护琛屽弬鏁帮紙绱㈠紩璺緞锛夛紝鍒涘缓骞舵樉绀轰富绐楀彛銆?
// ============================================================================

// Must include xapian before Qt to avoid keyword clashes
#include <xapian.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QDebug>

#include "gui/main_window.h"
#include "core/config.h"

// -----------------------------------------------------------------------
// main 鈥?涓诲叆鍙ｅ嚱鏁?
// -----------------------------------------------------------------------
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AnyTXT Searcher");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("AnyTXT");
    QApplication::setOrganizationDomain("anytxt.org");
    // Set window icon from the app resource
    app.setWindowIcon(QIcon(QStringLiteral(":/app.ico")));
    // Window icon loaded from app.rc resource


    QCommandLineParser parser;
    parser.setApplicationDescription("妗岄潰鍏ㄦ枃鎼滅储宸ュ叿");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption indexDirOption(
        QStringList() << "i" << "index-dir",
        "绱㈠紩鐩綍璺緞", "path"
    );
    parser.addOption(indexDirOption);

    parser.process(app);

    // 濡傛灉鍛戒护琛屾寚瀹氫簡绱㈠紩鐩綍鍒欎娇鐢紝鍚﹀垯浠庨厤缃枃浠跺姞杞?
    QString indexPath;
    if (parser.isSet(indexDirOption)) {
        indexPath = parser.value(indexDirOption);
    }
    MainWindow mainWindow(indexPath);
    mainWindow.show();
    return app.exec();
}


