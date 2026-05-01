// ============================================================================
// main.cpp — AnyTXT Searcher 入口文件
//
// 功能说明：
//   应用程序的主入口点。初始化 QApplication，设置应用元信息，
//   解析命令行参数（索引路径），创建并显示主窗口。
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
// main — 主入口函数
// -----------------------------------------------------------------------
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("AnyTXT Searcher");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("AnyTXT");
    QApplication::setOrganizationDomain("anytxt.org");
    // Window icon loaded from app.rc resource
    // Icon is loaded from app.rc resource for Windows native display
    // QApplication::windowIcon() will return the icon set by .rc file

    QCommandLineParser parser;
    parser.setApplicationDescription("桌面全文搜索工具");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption indexDirOption(
        QStringList() << "i" << "index-dir",
        "索引目录路径", "path"
    );
    parser.addOption(indexDirOption);

    parser.process(app);

    // 如果命令行指定了索引目录则使用，否则从配置文件加载
    QString indexPath;
    if (parser.isSet(indexDirOption)) {
        indexPath = parser.value(indexDirOption);
    }
    MainWindow mainWindow(indexPath);
    mainWindow.show();
    return app.exec();
}
