// Minimal ThemeConfig test - no QCoreApplication needed
// QJsonDocument, QColor, QFont, QFile all work without QApp

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
#include <cstring>

// Copy minimal ThemeConfig logic inline (header-only)
#include "core/theme_config.h"

static int g_passed = 0;
static int g_failed = 0;

#define CHECK(cond, msg) do { \
    if (cond) { g_passed++; printf("  [PASS] %s\n", msg); } \
    else { g_failed++; printf("  [FAIL] %s\n", msg); } \
} while(0)

int main()
{
    printf("==============================================\n");
    printf(" AnyTXT Theme System Quick Tests\n");
    printf("==============================================\n");

    // 1. Default construction
    {
        printf("\n--- Default construction ---\n");
        ThemeConfig empty;
        CHECK(empty.isValid(), "Default theme is valid (has default name)");
        CHECK(strcmp(empty.name.toUtf8().constData(), "System Default") == 0,
              "Default name: System Default");
        CHECK(empty.description == "System default theme", "Default description");
    }

    // 2. JSON parsing
    {
        printf("\n--- JSON parsing ---\n");
        QJsonObject root;
        root["name"] = "QuickTest";

        QJsonObject colors;
        QJsonObject window;
        window["background"] = "#ff0000";
        window["foreground"] = "#ffffff";
        colors["window"] = window;

        QJsonObject btn;
        btn["background"] = "#333333";
        btn["foreground"] = "#ffffff";
        btn["border"] = "#555555";
        btn["hover"] = "#444444";
        btn["pressed"] = "#222222";
        QJsonObject disabled;
        disabled["foreground"] = "#666666";
        disabled["background"] = "#2d2d2d";
        btn["disabled"] = disabled;
        colors["button"] = btn;

        QJsonObject toolbar;
        toolbar["background"] = "#2d2d2d";
        toolbar["foreground"] = "#ffffff";
        toolbar["border"] = "#333";
        toolbar["hover"] = "#3c3c3c";
        toolbar["pressed"] = "#094771";
        colors["toolbar"] = toolbar;

        QJsonObject statusbar;
        statusbar["background"] = "#007acc";
        statusbar["foreground"] = "#ffffff";
        colors["statusbar"] = statusbar;

        QJsonObject splitter;
        splitter["handle"] = "#333";
        colors["splitter"] = splitter;

        QJsonObject menubar;
        menubar["background"] = "#2d2d2d";
        menubar["foreground"] = "#ffffff";
        menubar["border"] = "#333";
        menubar["hover"] = "#094771";
        colors["menubar"] = menubar;

        QJsonObject input;
        input["background"] = "#3c3c3c";
        input["foreground"] = "#ffffff";
        input["border"] = "#555";
        input["focusBorder"] = "#1976D2";
        colors["input"] = input;

        QJsonObject tree;
        tree["background"] = "#1e1e1e";
        tree["foreground"] = "#ffffff";
        tree["alternateBackground"] = "#252525";
        tree["border"] = "#333";
        tree["selected"] = "#094771";
        tree["hover"] = "#2a2d2e";
        colors["tree"] = tree;

        QJsonObject list;
        list["background"] = "#1e1e1e";
        list["foreground"] = "#ffffff";
        list["border"] = "#333";
        list["selected"] = "#094771";
        list["hover"] = "#2a2d2e";
        colors["list"] = list;

        QJsonObject textedit;
        textedit["background"] = "#1e1e1e";
        textedit["foreground"] = "#ffffff";
        textedit["border"] = "#333";
        colors["textedit"] = textedit;

        QJsonObject tab;
        tab["paneBackground"] = "#1e1e1e";
        tab["background"] = "#2d2d2d";
        tab["foreground"] = "#ffffff";
        tab["border"] = "#333";
        tab["selectedBackground"] = "#1e1e1e";
        tab["selectedForeground"] = "#ffffff";
        tab["hoverBackground"] = "#3c3c3c";
        colors["tab"] = tab;

        QJsonObject progress;
        progress["background"] = "#2d2d2d";
        progress["border"] = "#555";
        progress["foreground"] = "#ffffff";
        progress["chunk"] = "#1976D2";
        colors["progress"] = progress;

        QJsonObject combo;
        combo["background"] = "#3c3c3c";
        combo["foreground"] = "#ffffff";
        combo["border"] = "#555";
        combo["itemHover"] = "#094771";
        colors["combo"] = combo;

        QJsonObject checkbox;
        checkbox["foreground"] = "#ffffff";
        colors["checkbox"] = checkbox;

        QJsonObject groupbox;
        groupbox["foreground"] = "#ffffff";
        colors["groupbox"] = groupbox;

        QJsonObject header;
        header["background"] = "#2d2d2d";
        colors["header"] = header;

        QJsonObject scrollbar;
        scrollbar["background"] = "#2d2d2d";
        scrollbar["handle"] = "#555";
        colors["scrollbar"] = scrollbar;

        root["colors"] = colors;

        QJsonObject fonts;
        QJsonObject def;
        def["family"] = "Segoe UI";
        def["size"] = 10;
        fonts["default"] = def;
        QJsonObject code;
        code["family"] = "Consolas";
        code["size"] = 10;
        fonts["code"] = code;
        root["fonts"] = fonts;

        QJsonObject effects;
        effects["buttonBorderRadius"] = "6px";
        root["effects"] = effects;

        ThemeConfig t(root);
        CHECK(t.isValid(), "Parsed theme valid");
        CHECK(strcmp(t.name.toUtf8().constData(), "QuickTest") == 0, "Theme name: QuickTest");

        CHECK(t.color("window", "background") == QColor("#ff0000"), "window bg = #ff0000");
        CHECK(t.color("window", "foreground") == QColor("#ffffff"), "window fg = #ffffff");
        CHECK(t.color("button", "background") == QColor("#333333"), "button bg = #333333");
        CHECK(t.color("button", "disabled.foreground") == QColor("#666666"), "disabled fg = #666666");
        CHECK(t.color("window", "nonexistent", QColor("#abc123")) == QColor("#abc123"), "missing color default");

        QFont appFont = t.applicationFont();
        CHECK(strcmp(appFont.family().toUtf8().constData(), "Segoe UI") == 0, "Font: Segoe UI");
        CHECK(appFont.pointSize() == 10, "Font size: 10");

        QFont cFont = t.codeFont();
        CHECK(strcmp(cFont.family().toUtf8().constData(), "Consolas") == 0, "Code font: Consolas");

        CHECK(strcmp(t.effect("buttonBorderRadius").toUtf8().constData(), "6px") == 0, "Effect: 6px");
        CHECK(strcmp(t.effect("nonexistent", "4px").toUtf8().constData(), "4px") == 0, "Missing effect default: 4px");
    }

    // 3. QSS generation - check all widgets present
    {
        printf("\n--- QSS generation ---\n");
        QJsonObject root;
        root["name"] = "QSSTest";

        QJsonObject colors;
        QJsonObject w; w["background"] = "#111"; w["foreground"] = "#eee";
        colors["window"] = w;

        QJsonObject bt; bt["background"] = "#333"; bt["foreground"] = "#eee";
        bt["border"] = "#555"; bt["hover"] = "#444"; bt["pressed"] = "#222";
        QJsonObject d; d["foreground"] = "#666"; d["background"] = "#222";
        bt["disabled"] = d;
        colors["button"] = bt;

        QJsonObject tb; tb["background"] = "#222"; tb["foreground"] = "#eee";
        tb["border"] = "#333"; tb["hover"] = "#333"; tb["pressed"] = "#094771";
        colors["toolbar"] = tb;

        QJsonObject sb; sb["background"] = "#007acc"; sb["foreground"] = "#fff";
        colors["statusbar"] = sb;

        QJsonObject mb; mb["background"] = "#222"; mb["foreground"] = "#eee";
        mb["border"] = "#333"; mb["hover"] = "#094771";
        colors["menubar"] = mb;

        QJsonObject me; me["background"] = "#222"; me["foreground"] = "#eee";
        me["border"] = "#333"; me["hover"] = "#094771"; me["separator"] = "#333";
        colors["menu"] = me;

        QJsonObject sp; sp["handle"] = "#333";
        colors["splitter"] = sp;

        QJsonObject inp; inp["background"] = "#333"; inp["foreground"] = "#eee";
        inp["border"] = "#555"; inp["focusBorder"] = "#1976D2";
        colors["input"] = inp;

        QJsonObject tr; tr["background"] = "#111"; tr["foreground"] = "#eee";
        tr["alternateBackground"] = "#181818"; tr["border"] = "#333";
        tr["selected"] = "#094771"; tr["hover"] = "#222";
        colors["tree"] = tr;

        QJsonObject li; li["background"] = "#111"; li["foreground"] = "#eee";
        li["border"] = "#333"; li["selected"] = "#094771"; li["hover"] = "#222";
        colors["list"] = li;

        QJsonObject te; te["background"] = "#111"; te["foreground"] = "#eee";
        te["border"] = "#333";
        colors["textedit"] = te;

        QJsonObject ta; ta["paneBackground"] = "#111"; ta["background"] = "#222";
        ta["foreground"] = "#eee"; ta["border"] = "#333";
        ta["selectedBackground"] = "#111"; ta["selectedForeground"] = "#eee";
        ta["hoverBackground"] = "#333";
        colors["tab"] = ta;

        QJsonObject pr; pr["background"] = "#222"; pr["border"] = "#555";
        pr["foreground"] = "#eee"; pr["chunk"] = "#1976D2";
        colors["progress"] = pr;

        QJsonObject co; co["background"] = "#333"; co["foreground"] = "#eee";
        co["border"] = "#555"; co["itemHover"] = "#094771";
        colors["combo"] = co;

        QJsonObject ch; ch["foreground"] = "#eee";
        colors["checkbox"] = ch;

        QJsonObject gr; gr["foreground"] = "#eee";
        colors["groupbox"] = gr;

        QJsonObject he; he["background"] = "#222";
        colors["header"] = he;

        QJsonObject sc; sc["background"] = "#222"; sc["handle"] = "#555";
        colors["scrollbar"] = sc;

        root["colors"] = colors;

        ThemeConfig t(root);
        QString css = t.toStyleSheet();
        int len = css.length();
        printf("  [INFO] QSS length: %d chars\n", len);

        CHECK(css.contains("QMainWindow"), "QMainWindow in QSS");
        CHECK(css.contains("QPushButton"), "QPushButton in QSS");
        CHECK(css.contains("QToolBar"), "QToolBar in QSS");
        CHECK(css.contains("QStatusBar"), "QStatusBar in QSS");
        CHECK(css.contains("QLineEdit"), "QLineEdit in QSS");
        CHECK(css.contains("QTreeWidget"), "QTreeWidget in QSS");
        CHECK(css.contains("QListWidget"), "QListWidget in QSS");
        CHECK(css.contains("QTextEdit"), "QTextEdit in QSS");
        CHECK(css.contains("QTabWidget"), "QTabWidget in QSS");
        CHECK(css.contains("QProgressBar"), "QProgressBar in QSS");
        CHECK(css.contains("QComboBox"), "QComboBox in QSS");
        CHECK(css.contains("QCheckBox"), "QCheckBox in QSS");
        CHECK(css.contains("QGroupBox"), "QGroupBox in QSS");
        CHECK(css.contains("QSpinBox"), "QSpinBox in QSS");
        CHECK(css.contains("QMenuBar"), "QMenuBar in QSS");
        CHECK(css.contains("QMenu"), "QMenu in QSS");
        CHECK(css.contains("QSplitter"), "QSplitter in QSS");

        CHECK(css.contains("QPushButton:hover"), "hover pseudostate");
        CHECK(css.contains("QPushButton:pressed"), "pressed pseudostate");
        CHECK(css.contains("QPushButton:disabled"), "disabled pseudostate");
        CHECK(css.contains("QScrollBar:vertical"), "vertical scrollbar");
        CHECK(css.contains("QScrollBar:horizontal"), "horizontal scrollbar");
        CHECK(css.contains("QTreeWidget::item:selected"), "tree selected");
        CHECK(css.contains("QTabBar::tab:selected"), "tab selected");
        CHECK(css.contains(":focus"), "focus state");
    }

    // 4. Border conditions
    {
        printf("\n--- Edge cases ---\n");
        // Minimal valid theme
        QJsonObject min;
        min["name"] = "Minimal";
        min["colors"] = QJsonObject();
        ThemeConfig minimal(min);
        CHECK(minimal.isValid(), "Minimal config valid");
        CHECK(!minimal.toStyleSheet().isEmpty(), "Minimal QSS non-empty");

        // Empty bare theme
        QJsonObject bare;
        bare["name"] = "Bare";
        ThemeConfig bareTheme(bare);
        CHECK(bareTheme.isValid(), "Bare theme valid");
        CHECK(bareTheme.trayIconPath.isEmpty(), "trayIcon empty");
        CHECK(bareTheme.effect("missing") == "", "Missing effect = empty string");
    }

    // 5. JSON validation
    {
        printf("\n--- JSON validation ---\n");
        QJsonObject valid;
        valid["name"] = "Valid";
        QString err;
        CHECK(ThemeConfig::validateJson(valid, &err), "Valid JSON passes");

        QJsonObject noName;
        CHECK(!ThemeConfig::validateJson(noName, &err), "No name rejected");

        QJsonObject emptyName;
        emptyName["name"] = "";
        CHECK(!ThemeConfig::validateJson(emptyName), "Empty name rejected");

        QJsonObject badColors;
        badColors["name"] = "Test";
        badColors["colors"] = "not_object";
        CHECK(!ThemeConfig::validateJson(badColors), "Non-object colors rejected");
    }

    // 6. File save/load
    {
        printf("\n--- File I/O ---\n");
        QJsonObject root;
        root["name"] = "FileIOTest";
        QJsonObject colors;
        QJsonObject window;
        window["background"] = "#aabbcc";
        colors["window"] = window;
        root["colors"] = colors;

        ThemeConfig t(root);
        QString tmpPath = QDir::tempPath() + "/anytxt_theme_io_test.json";
        CHECK(t.saveToFile(tmpPath), "Save file");
        CHECK(QFile::exists(tmpPath), "File exists");

        ThemeConfig loaded = ThemeConfig::fromFile(tmpPath);
        CHECK(loaded.isValid(), "Loaded theme valid");
        CHECK(strcmp(loaded.name.toUtf8().constData(), "FileIOTest") == 0, "Name matches");
        CHECK(loaded.color("window", "background") == QColor("#aabbcc"), "Color matches");

        QFile::remove(tmpPath);
        CHECK(!QFile::exists(tmpPath), "Cleanup OK");
    }

    printf("\n==============================================\n");
    printf(" Results: %d passed, %d failed\n", g_passed, g_failed);
    printf("==============================================\n");

    return g_failed > 0 ? 1 : 0;
}
