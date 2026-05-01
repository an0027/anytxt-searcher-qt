/*
 * test_theme.cpp - 主题系统集成测试（无 QApplication 依赖）
 *
 * 测试项：
 *   1. ThemeConfig 从 JSON 解析
 *   2. QSS 样式表生成不崩溃，包含全部控件
 *   3. 内置主题正确加载
 *   4. 自定义主题文件扫描
 *   5. 主题切换不崩溃
 *   6. JSON 校验逻辑
 *   7. 文件保存/加载
 *
 * 注意：本测试不使用 QApplication，避免了 Windows 无桌面环境的问题。
 *      QColor、QFont、QJsonDocument 等均在无 QApp 时可用。
 */

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QColor>
#include <QFont>
#include <cstdio>
#include <cstdlib>

#include "core/theme_config.h"

static int g_passed = 0;
static int g_failed = 0;

#define TEST(name) qDebug() << "\n=== " << name << " ==="

#define CHECK(cond, msg) do { \
    if (cond) { g_passed++; qDebug() << "  [PASS]" << msg; } \
    else { g_failed++; qWarning() << "  [FAIL]" << msg; } \
} while(0)

int main(int argc, char* argv[])
{
    // Minimal QCoreApplication for QSettings compatibility
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("AnyTXT Theme Test");
    QCoreApplication::setOrganizationName("AnyTXT");

    printf("==============================================\n");
    printf(" AnyTXT Theme System Tests\n");
    printf("==============================================\n");
    fflush(stdout);

    // ================================================================
    // 1. 默认构造
    // ================================================================
    {
        TEST("默认构造");
        ThemeConfig empty;
        CHECK(!empty.isValid(), "空主题应无效");
        CHECK(empty.name == "System Default", "默认名称");
    }

    // ================================================================
    // 2. JSON 解析 + 颜色/字体/效果访问
    // ================================================================
    {
        TEST("JSON 解析与颜色访问");

        QJsonObject root;
        root["name"] = "TestTheme";
        root["description"] = "A test theme";

        QJsonObject colors;
        QJsonObject window; window["background"] = "#ff0000"; window["foreground"] = "#ffffff";
        colors["window"] = window;

        QJsonObject btn; btn["background"] = "#333333"; btn["foreground"] = "#ffffff";
        btn["border"] = "#555555"; btn["hover"] = "#444444"; btn["pressed"] = "#222222";
        QJsonObject disabled; disabled["foreground"] = "#666666"; disabled["background"] = "#2d2d2d";
        btn["disabled"] = disabled;
        colors["button"] = btn;

        QJsonObject toolbar; toolbar["background"] = "#2d2d2d"; toolbar["foreground"] = "#ffffff";
        toolbar["border"] = "#333"; toolbar["hover"] = "#3c3c3c"; toolbar["pressed"] = "#094771";
        colors["toolbar"] = toolbar;

        QJsonObject statusbar; statusbar["background"] = "#007acc"; statusbar["foreground"] = "#ffffff";
        colors["statusbar"] = statusbar;
        QJsonObject splitter; splitter["handle"] = "#333";
        colors["splitter"] = splitter;

        QJsonObject menubar; menubar["background"] = "#2d2d2d"; menubar["foreground"] = "#ffffff";
        menubar["border"] = "#333"; menubar["hover"] = "#094771";
        colors["menubar"] = menubar;

        QJsonObject input; input["background"] = "#3c3c3c"; input["foreground"] = "#ffffff";
        input["border"] = "#555"; input["focusBorder"] = "#1976D2";
        colors["input"] = input;

        QJsonObject tree; tree["background"] = "#1e1e1e"; tree["foreground"] = "#ffffff";
        tree["alternateBackground"] = "#252525"; tree["border"] = "#333";
        tree["selected"] = "#094771"; tree["hover"] = "#2a2d2e";
        colors["tree"] = tree;

        QJsonObject list; list["background"] = "#1e1e1e"; list["foreground"] = "#ffffff";
        list["border"] = "#333"; list["selected"] = "#094771"; list["hover"] = "#2a2d2e";
        colors["list"] = list;

        QJsonObject textedit; textedit["background"] = "#1e1e1e"; textedit["foreground"] = "#ffffff";
        textedit["border"] = "#333";
        colors["textedit"] = textedit;

        QJsonObject tab; tab["paneBackground"] = "#1e1e1e"; tab["background"] = "#2d2d2d";
        tab["foreground"] = "#ffffff"; tab["border"] = "#333";
        tab["selectedBackground"] = "#1e1e1e"; tab["selectedForeground"] = "#ffffff";
        tab["hoverBackground"] = "#3c3c3c";
        colors["tab"] = tab;

        QJsonObject progress; progress["background"] = "#2d2d2d"; progress["border"] = "#555";
        progress["foreground"] = "#ffffff"; progress["chunk"] = "#1976D2";
        colors["progress"] = progress;

        QJsonObject combo; combo["background"] = "#3c3c3c"; combo["foreground"] = "#ffffff";
        combo["border"] = "#555"; combo["itemHover"] = "#094771";
        colors["combo"] = combo;

        QJsonObject checkbox; checkbox["foreground"] = "#ffffff";
        colors["checkbox"] = checkbox;

        QJsonObject groupbox; groupbox["foreground"] = "#ffffff";
        colors["groupbox"] = groupbox;

        QJsonObject header; header["background"] = "#2d2d2d";
        colors["header"] = header;

        QJsonObject scrollbar; scrollbar["background"] = "#2d2d2d"; scrollbar["handle"] = "#555";
        colors["scrollbar"] = scrollbar;

        root["colors"] = colors;

        QJsonObject fonts;
        QJsonObject def; def["family"] = "Segoe UI"; def["size"] = 10;
        fonts["default"] = def;
        QJsonObject code; code["family"] = "Consolas"; code["size"] = 10;
        fonts["code"] = code;
        root["fonts"] = fonts;

        QJsonObject effects;
        effects["buttonBorderRadius"] = "6px";
        root["effects"] = effects;

        ThemeConfig t(root);
        CHECK(t.isValid(), "解析后的主题有效");
        CHECK(t.name == "TestTheme", "主题名称正确");
        CHECK(t.description == "A test theme", "描述正确");

        CHECK(t.color("window", "background") == QColor("#ff0000"), "Window 背景色");
        CHECK(t.color("button", "background") == QColor("#333333"), "Button 背景色");
        CHECK(t.color("button", "disabled.foreground") == QColor("#666666"), "Button disabled 前景色 (嵌套路径)");
        CHECK(t.color("window", "nonexistent", QColor("#abc123")) == QColor("#abc123"), "缺失颜色返回默认值");

        QFont appFont = t.applicationFont();
        CHECK(appFont.family() == "Segoe UI", "应用字体: " + appFont.family().toLatin1());
        CHECK(appFont.pointSize() == 10, "应用字号: 10");

        QFont cFont = t.codeFont();
        CHECK(cFont.family() == "Consolas", "代码字体: " + cFont.family().toLatin1());

        CHECK(t.effect("buttonBorderRadius") == "6px", "按钮圆角 6px");
        CHECK(t.effect("nonexistent", "4px") == "4px", "缺失效果返回默认值");
    }

    // ================================================================
    // 3. QSS 样式表生成
    // ================================================================
    {
        TEST("QSS 样式表生成 (全控件覆盖)");

        QJsonObject root;
        root["name"] = "QSSTest";

        QJsonObject colors;
        QJsonObject window; window["background"] = "#1e1e1e"; window["foreground"] = "#d4d4d4";
        colors["window"] = window;
        QJsonObject button; button["background"] = "#3c3c3c"; button["foreground"] = "#d4d4d4";
        button["border"] = "#555"; button["hover"] = "#4c4c4c"; button["pressed"] = "#094771";
        QJsonObject disabled; disabled["foreground"] = "#666"; disabled["background"] = "#2d2d2d";
        button["disabled"] = disabled;
        colors["button"] = button;
        QJsonObject toolbar; toolbar["background"] = "#2d2d2d"; toolbar["foreground"] = "#d4d4d4";
        toolbar["border"] = "#3c3c3c"; toolbar["hover"] = "#3c3c3c"; toolbar["pressed"] = "#094771";
        colors["toolbar"] = toolbar;
        QJsonObject statusbar; statusbar["background"] = "#007acc"; statusbar["foreground"] = "#ffffff";
        colors["statusbar"] = statusbar;
        QJsonObject menubar; menubar["background"] = "#2d2d2d"; menubar["foreground"] = "#d4d4d4";
        menubar["border"] = "#3c3c3c"; menubar["hover"] = "#094771";
        colors["menubar"] = menubar;
        QJsonObject menu; menu["background"] = "#2d2d2d"; menu["foreground"] = "#d4d4d4";
        menu["border"] = "#3c3c3c"; menu["hover"] = "#094771"; menu["separator"] = "#3c3c3c";
        colors["menu"] = menu;
        QJsonObject splitter; splitter["handle"] = "#3c3c3c";
        colors["splitter"] = splitter;
        QJsonObject input; input["background"] = "#3c3c3c"; input["foreground"] = "#d4d4d4";
        input["border"] = "#555"; input["focusBorder"] = "#1976D2";
        colors["input"] = input;
        QJsonObject tree; tree["background"] = "#1e1e1e"; tree["foreground"] = "#d4d4d4";
        tree["alternateBackground"] = "#252525"; tree["border"] = "#3c3c3c";
        tree["selected"] = "#094771"; tree["hover"] = "#2a2d2e";
        colors["tree"] = tree;
        QJsonObject list; list["background"] = "#1e1e1e"; list["foreground"] = "#d4d4d4";
        list["border"] = "#3c3c3c"; list["selected"] = "#094771"; list["hover"] = "#2a2d2e";
        colors["list"] = list;
        QJsonObject textedit; textedit["background"] = "#1e1e1e"; textedit["foreground"] = "#d4d4d4";
        textedit["border"] = "#3c3c3c";
        colors["textedit"] = textedit;
        QJsonObject tab; tab["paneBackground"] = "#1e1e1e"; tab["background"] = "#2d2d2d";
        tab["foreground"] = "#d4d4d4"; tab["border"] = "#3c3c3c";
        tab["selectedBackground"] = "#1e1e1e"; tab["selectedForeground"] = "#d4d4d4";
        tab["hoverBackground"] = "#3c3c3c";
        colors["tab"] = tab;
        QJsonObject progress; progress["background"] = "#2d2d2d"; progress["border"] = "#555";
        progress["foreground"] = "#d4d4d4"; progress["chunk"] = "#1976D2";
        colors["progress"] = progress;
        QJsonObject combo; combo["background"] = "#3c3c3c"; combo["foreground"] = "#d4d4d4";
        combo["border"] = "#555"; combo["itemHover"] = "#094771";
        colors["combo"] = combo;
        QJsonObject checkbox; checkbox["foreground"] = "#d4d4d4";
        colors["checkbox"] = checkbox;
        QJsonObject groupbox; groupbox["foreground"] = "#d4d4d4";
        colors["groupbox"] = groupbox;
        QJsonObject header; header["background"] = "#2d2d2d";
        colors["header"] = header;
        QJsonObject scrollbar; scrollbar["background"] = "#2d2d2d"; scrollbar["handle"] = "#555";
        colors["scrollbar"] = scrollbar;
        root["colors"] = colors;

        ThemeConfig t(root);
        QString css = t.toStyleSheet();

        // Verify QSS contains all control types
        CHECK(css.contains("QMainWindow"), "QMainWindow");
        CHECK(css.contains("QPushButton"), "QPushButton");
        CHECK(css.contains("QToolBar"), "QToolBar");
        CHECK(css.contains("QStatusBar"), "QStatusBar");
        CHECK(css.contains("QLineEdit"), "QLineEdit");
        CHECK(css.contains("QTreeWidget"), "QTreeWidget");
        CHECK(css.contains("QListWidget"), "QListWidget");
        CHECK(css.contains("QTextEdit"), "QTextEdit");
        CHECK(css.contains("QTabWidget"), "QTabWidget");
        CHECK(css.contains("QProgressBar"), "QProgressBar");
        CHECK(css.contains("QComboBox"), "QComboBox");
        CHECK(css.contains("QCheckBox"), "QCheckBox");
        CHECK(css.contains("QGroupBox"), "QGroupBox");
        CHECK(css.contains("QSpinBox"), "QSpinBox");
        CHECK(css.contains("QMenuBar"), "QMenuBar");
        CHECK(css.contains("QMenu"), "QMenu");
        CHECK(css.contains("QSplitter"), "QSplitter");

        // Verify pseudo states
        CHECK(css.contains("QPushButton:hover"), "hover 伪状态");
        CHECK(css.contains("QPushButton:pressed"), "pressed 伪状态");
        CHECK(css.contains("QPushButton:disabled"), "disabled 伪状态");
        CHECK(css.contains("QScrollBar:vertical"), "垂直滚动条");
        CHECK(css.contains("QScrollBar:horizontal"), "水平滚动条");
        CHECK(css.contains("QTreeWidget::item:selected"), "Tree 选中状态");
        CHECK(css.contains("QTabBar::tab:selected"), "Tab 选中状态");
        CHECK(css.contains(":focus"), "focus 伪状态");

        // Verify colors embedded in QSS
        CHECK(css.contains("#1e1e1e"), "背景色 #1e1e1e");
        CHECK(css.contains("#d4d4d4"), "前景色 #d4d4d4");
        CHECK(css.contains("#094771"), "选择色 #094771");
        CHECK(css.contains("#1976D2"), "焦点色 #1976D2");

        qDebug() << "  [INFO] QSS length:" << css.length() << "chars";
    }

    // ================================================================
    // 4. 内置主题加载 + ThemeManager
    // ================================================================
    {
        TEST("ThemeManager 内置主题");

        ThemeManager mgr;
        mgr.initialize();

        QStringList themes = mgr.availableThemes();
        CHECK(themes.size() >= 3, "至少 3 个内置主题, 实际 " + QByteArray::number(themes.size()));

        CHECK(mgr.theme("default") != nullptr, "default 主题存在");
        CHECK(mgr.theme("dark") != nullptr, "dark 主题存在");
        CHECK(mgr.theme("light") != nullptr, "light 主题存在");
        CHECK(mgr.theme("nonexistent") == nullptr, "不存在的主题返回 nullptr");

        CHECK(mgr.currentTheme().isValid(), "当前主题有效");
        // After init, default is "dark"
        const auto& cur = mgr.currentTheme();
        CHECK(cur.name == "Dark", "默认主题为 Dark, 实际: " + cur.name.toLatin1());

        // Theme switching
        CHECK(mgr.setTheme("light"), "切换到 light 主题");
        CHECK(mgr.currentThemeName() == "light", "当前主题 key 为 'light'");

        CHECK(mgr.setTheme("default"), "切换到 default 主题");
        CHECK(mgr.currentTheme().name == QString::fromUtf8("系统默认"), "default 主题名称正确");

        CHECK(!mgr.setTheme("nonexistent"), "切换不存在的主题返回 false");

        // Re-apply (should not crash)
        mgr.applyTheme();
        CHECK(true, "重新应用当前主题无崩溃");
    }

    // ================================================================
    // 5. JSON 校验
    // ================================================================
    {
        TEST("JSON 格式校验");

        QJsonObject valid;
        valid["name"] = "Valid";
        QString err;
        CHECK(ThemeConfig::validateJson(valid, &err), "有效 JSON 通过: " + err.toLatin1());

        QJsonObject noName;
        CHECK(!ThemeConfig::validateJson(noName, &err), "缺少 name 被拒绝");
        CHECK(!err.isEmpty(), "错误信息非空");

        QJsonObject emptyName;
        emptyName["name"] = "";
        CHECK(!ThemeConfig::validateJson(emptyName), "空 name 被拒绝");

        QJsonObject badColors;
        badColors["name"] = "Test";
        badColors["colors"] = "not_an_object";
        CHECK(!ThemeConfig::validateJson(badColors), "非 object 的 colors 被拒绝");
    }

    // ================================================================
    // 6. 文件保存/加载
    // ================================================================
    {
        TEST("JSON 文件读写");

        QJsonObject root;
        root["name"] = "FileTest";
        QJsonObject colors;
        QJsonObject window; window["background"] = "#abcdef";
        colors["window"] = window;
        root["colors"] = colors;

        ThemeConfig t(root);
        QString tmpPath = QDir::tempPath() + "/anytxt_theme_test.json";
        CHECK(t.saveToFile(tmpPath), "保存主题文件: " + tmpPath.toLatin1());
        CHECK(QFile::exists(tmpPath), "文件已存在");

        ThemeConfig loaded = ThemeConfig::fromFile(tmpPath);
        CHECK(loaded.isValid(), "加载的主题有效");
        CHECK(loaded.name == "FileTest", "加载的名称匹配");
        CHECK(loaded.color("window", "background") == QColor("#abcdef"), "加载的颜色匹配");

        QFile::remove(tmpPath);
        CHECK(!QFile::exists(tmpPath), "清理测试文件");
    }

    // ================================================================
    // 7. 空颜色/无 colors 部分
    // ================================================================
    {
        TEST("边界条件");

        // Empty colors section
        QJsonObject root;
        root["name"] = "Minimal";
        root["colors"] = QJsonObject();
        ThemeConfig minimal(root);
        CHECK(minimal.isValid(), "最小配置有效");
        QString css = minimal.toStyleSheet();
        CHECK(!css.isEmpty(), "QSS 非空（使用默认颜色）");
        CHECK(css.contains("QMainWindow"), "QSS 仍包含基础控件");

        // No colors, no fonts
        QJsonObject bare;
        bare["name"] = "Bare";
        ThemeConfig bareTheme(bare);
        CHECK(bareTheme.isValid(), "裸主题有效");
        CHECK(bareTheme.trayIconPath.isEmpty(), "trayIcon 为空");
        CHECK(bareTheme.effect("nonexistent") == "", "缺失 effect 返回空串");
    }

    // ================================================================
    // Summary
    // ================================================================
    qDebug() << "\n==============================================";
    qDebug() << " Results:" << g_passed << "passed," << g_failed << "failed";
    qDebug() << "==============================================";

    return g_failed > 0 ? 1 : 0;
}
