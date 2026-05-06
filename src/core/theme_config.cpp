/*
 * theme_config.cpp - AnyTXT Searcher 主题配置实现
 *
 * 实现注释：
 *   主题系统基于 JSON 格式配置文件。
 *   内置三个默认主题（系统默认/暗色/亮色），用户在 themes/
 *   目录下添加 .json 文件即可自定义主题。
 *   主题变更时自动重新生成完整 QSS 样式表，通过 qApp->setStyleSheet()
 *   应用到整个应用程序。
 *
 *   QSS 生成逻辑：根据 JSON 中的颜色/字体/效果配置，
 *   为每种 Qt 控件类型生成对应的样式规则。
 *   未在 JSON 中指定的属性会使用 Qt 默认样式。
 */

#include "core/theme_config.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFont>
#include <QStandardPaths>
#include <QFileSystemWatcher>
#include <QSettings>

// ============================================================================
// ThemeConfig 实现
// ============================================================================

ThemeConfig::ThemeConfig()
    : name("System Default")
    , description("System default theme")
{
}

ThemeConfig::ThemeConfig(const QJsonObject& json)
{
    parse(json);
}

void ThemeConfig::parse(const QJsonObject& json)
{
    m_rawJson = json;
    name = json.value("name").toString("Unnamed Theme");
    description = json.value("description").toString();
    trayIconPath = json.value("trayIcon").toString();
    m_colors = json.value("colors").toObject();
    m_fonts = json.value("fonts").toObject();
    m_effects = json.value("effects").toObject();
}

QString ThemeConfig::resolveNested(const QJsonObject& root, const QString& dottedPath,
                                    const QString& defaultVal) const
{
    QStringList parts = dottedPath.split('.');
    QJsonObject obj = root;
    for (int i = 0; i < parts.size() - 1; ++i) {
        if (obj.contains(parts[i]) && obj[parts[i]].isObject()) {
            obj = obj[parts[i]].toObject();
        } else {
            return defaultVal;
        }
    }
    return obj.value(parts.last()).toString(defaultVal);
}

QColor ThemeConfig::color(const QString& category, const QString& key,
                           const QColor& defaultVal) const
{
    QString dotted = category + "." + key;
    QString val = resolveNested(m_colors, dotted);
    if (val.isEmpty()) return defaultVal;
    QColor c(val);
    return c.isValid() ? c : defaultVal;
}

QFont ThemeConfig::font(const QString& key, const QFont& defaultVal) const
{
    QJsonObject f = m_fonts.value(key).toObject();
    if (f.isEmpty()) return defaultVal;
    QFont font;
    font.setFamily(f.value("family").toString(defaultVal.family()));
    font.setPointSize(f.value("size").toInt(defaultVal.pointSize()));
    if (f.value("bold").toBool(false)) font.setBold(true);
    if (f.value("italic").toBool(false)) font.setItalic(true);
    return font;
}

QString ThemeConfig::effect(const QString& key, const QString& defaultVal) const
{
    return m_effects.value(key).toString(defaultVal);
}

QString ThemeConfig::resolvedTrayIconPath(const QString& themeDir) const
{
    if (trayIconPath.isEmpty()) return QString();
    QFileInfo fi(trayIconPath);
    if (fi.isAbsolute()) return trayIconPath;
    return themeDir + "/" + trayIconPath;
}

QFont ThemeConfig::applicationFont() const
{
    return font("default", QFont("Segoe UI", 10));
}

QFont ThemeConfig::codeFont() const
{
    return font("code", QFont("Consolas", 10));
}

QString ThemeConfig::generateButtonStyle() const
{
    QString bg      = color("button", "background", QColor("#3c3c3c")).name();
    QString fg      = color("button", "foreground", QColor("#d4d4d4")).name();
    QString border  = color("button", "border", QColor("#555555")).name();
    QString hover   = color("button", "hover", QColor("#4c4c4c")).name();
    QString pressed = color("button", "pressed", QColor("#094771")).name();
    QString disabledFg = color("button", "disabled.foreground", QColor("#666666")).name();
    QString disabledBg = color("button", "disabled.background", QColor("#2d2d2d")).name();
    QString radius = effect("buttonBorderRadius", "4px");
    QString transition = effect("transition", QString());

    QString css = QString(
        "QPushButton {"
        "  background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: %4; padding: 6px 16px;"
        "  %5"
        "}"
    ).arg(bg, fg, border, radius,
          transition.isEmpty() ? "" :
          QString("transition: %1; -qt-transition: %1;").arg(transition));

    css += QString(
        "QPushButton:hover { background-color: %1; }"
        "QPushButton:pressed { background-color: %2; }"
        "QPushButton:disabled { color: %3; background-color: %4; border-color: %5; }"
    ).arg(hover, pressed, disabledFg, disabledBg, disabledBg);

    return css;
}

QString ThemeConfig::generateToolbarStyle() const
{
    QString bg = color("toolbar", "background", QColor("#2d2d2d")).name();
    QString fg = color("toolbar", "foreground", QColor("#d4d4d4")).name();
    QString border = color("toolbar", "border", QColor("#3c3c3c")).name();
    QString hover = color("toolbar", "hover", QColor("#3c3c3c")).name();
    QString pressed = color("toolbar", "pressed", QColor("#094771")).name();

    return QString(
        "QToolBar { background-color: %1; border: none; border-bottom: 1px solid %2;"
        "  spacing: 4px; padding: 2px; }"
        "QToolBar QToolButton { color: %3; border: none; padding: 4px 8px;"
        "  border-radius: 3px; }"
        "QToolBar QToolButton:hover { background-color: %4; }"
        "QToolBar QToolButton:pressed { background-color: %5; }"
    ).arg(bg, border, fg, hover, pressed);
}

QString ThemeConfig::generateStatusBarStyle() const
{
    QString bg = color("statusbar", "background", QColor("#007acc")).name();
    QString fg = color("statusbar", "foreground", QColor("#ffffff")).name();
    QString fontSize = effect("statusbarFontSize", "12px");

    return QString(
        "QStatusBar { background-color: %1; color: %2; font-size: %3; }"
        "QStatusBar QLabel { color: %2; }"
    ).arg(bg, fg, fontSize);
}

QString ThemeConfig::generateSplitterStyle() const
{
    QString handle = color("splitter", "handle", QColor("#3c3c3c")).name();
    return QString(
        "QSplitter::handle { background-color: %1; }"
    ).arg(handle);
}

QString ThemeConfig::generateMenuStyle() const
{
    QString mbg = color("menubar", "background", QColor("#2d2d2d")).name();
    QString mfg = color("menubar", "foreground", QColor("#d4d4d4")).name();
    QString mborder = color("menubar", "border", QColor("#3c3c3c")).name();
    QString mhover = color("menubar", "hover", QColor("#094771")).name();

    QString bg = color("menu", "background", QColor("#2d2d2d")).name();
    QString fg = color("menu", "foreground", QColor("#d4d4d4")).name();
    QString border = color("menu", "border", QColor("#3c3c3c")).name();
    QString hover = color("menu", "hover", QColor("#094771")).name();
    QString sep = color("menu", "separator", QColor("#3c3c3c")).name();

    return QString(
        "QMenuBar { background-color: %1; color: %2; border-bottom: 1px solid %3; }"
        "QMenuBar::item:selected { background-color: %4; }"
        "QMenu { background-color: %5; color: %6; border: 1px solid %7; }"
        "QMenu::item:selected { background-color: %8; }"
        "QMenu::separator { height: 1px; background: %9; margin: 4px 8px; }"
    ).arg(mbg, mfg, mborder, mhover,
          bg, fg, border, hover, sep);
}

QString ThemeConfig::generateInputStyle() const
{
    QString bg = color("input", "background", QColor("#3c3c3c")).name();
    QString fg = color("input", "foreground", QColor("#d4d4d4")).name();
    QString border = color("input", "border", QColor("#555555")).name();
    QString focusBorder = color("input", "focusBorder", QColor("#1976D2")).name();

    return QString(
        "QLineEdit { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 4px 8px; }"
        "QLineEdit:focus { border-color: %4; }"
    ).arg(bg, fg, border, focusBorder);
}

QString ThemeConfig::generateTreeStyle() const
{
    QString bg = color("tree", "background", QColor("#1e1e1e")).name();
    QString fg = color("tree", "foreground", QColor("#d4d4d4")).name();
    QString altBg = color("tree", "alternateBackground", QColor("#252525")).name();
    QString border = color("tree", "border", QColor("#3c3c3c")).name();
    QString selected = color("tree", "selected", QColor("#094771")).name();
    QString hover = color("tree", "hover", QColor("#2a2d2e")).name();

    return QString(
        "QTreeWidget { background-color: %1; color: %2; border: 1px solid %3;"
        "  alternate-background-color: %4; }"
        "QTreeWidget::item:selected { background-color: %5; }"
        "QTreeWidget::item:hover { background-color: %6; }"
        "QHeaderView::section { background-color: %7; color: %2;"
        "  border: 1px solid %3; padding: 4px; }"
    ).arg(bg, fg, border, altBg, selected, hover,
          color("header", "background", QColor("#2d2d2d")).name());
}

QString ThemeConfig::generateListStyle() const
{
    QString bg = color("list", "background", QColor("#1e1e1e")).name();
    QString fg = color("list", "foreground", QColor("#d4d4d4")).name();
    QString border = color("list", "border", QColor("#3c3c3c")).name();
    QString selected = color("list", "selected", QColor("#094771")).name();
    QString hover = color("list", "hover", QColor("#2a2d2e")).name();

    return QString(
        "QListWidget { background-color: %1; color: %2; border: 1px solid %3; }"
        "QListWidget::item:selected { background-color: %4; }"
        "QListWidget::item:hover { background-color: %5; }"
    ).arg(bg, fg, border, selected, hover);
}

QString ThemeConfig::generateTextEditStyle() const
{
    QString bg = color("textedit", "background", QColor("#1e1e1e")).name();
    QString fg = color("textedit", "foreground", QColor("#d4d4d4")).name();
    QString border = color("textedit", "border", QColor("#3c3c3c")).name();

    return QString(
        "QTextEdit { background-color: %1; color: %2; border: 1px solid %3; }"
    ).arg(bg, fg, border);
}

QString ThemeConfig::generateTabStyle() const
{
    QString paneBg = color("tab", "paneBackground", color("window", "background", QColor("#1e1e1e")).name()).name();
    QString paneBorder = color("tab", "border", QColor("#3c3c3c")).name();
    QString tabBg = color("tab", "background", QColor("#2d2d2d")).name();
    QString tabFg = color("tab", "foreground", QColor("#d4d4d4")).name();
    QString tabBorder = color("tab", "border", QColor("#3c3c3c")).name();
    QString selectedBg = color("tab", "selectedBackground", QColor("#1e1e1e")).name();
    QString selectedFg = color("tab", "selectedForeground", QColor("#d4d4d4")).name();
    QString hoverBg = color("tab", "hoverBackground", QColor("#3c3c3c")).name();

    return QString(
        "QTabWidget::pane { background-color: %1; border: 1px solid %2; }"
        "QTabBar::tab { background-color: %3; color: %4; padding: 6px 16px;"
        "  border: 1px solid %5; border-bottom: none;"
        "  border-top-left-radius: 4px; border-top-right-radius: 4px; }"
        "QTabBar::tab:selected { background-color: %6; color: %7;"
        "  border-bottom: 1px solid %6; }"
        "QTabBar::tab:hover { background-color: %8; }"
    ).arg(paneBg, paneBorder,
          tabBg, tabFg, tabBorder,
          selectedBg, selectedFg, hoverBg);
}

QString ThemeConfig::generateProgressStyle() const
{
    QString bg = color("progress", "background", QColor("#2d2d2d")).name();
    QString border = color("progress", "border", QColor("#555555")).name();
    QString fg = color("progress", "foreground", QColor("#d4d4d4")).name();
    QString chunk = color("progress", "chunk", QColor("#1976D2")).name();

    return QString(
        "QProgressBar { background-color: %1; border: 1px solid %2;"
        "  border-radius: 3px; text-align: center; color: %3; }"
        "QProgressBar::chunk { background-color: %4; border-radius: 2px; }"
    ).arg(bg, border, fg, chunk);
}

QString ThemeConfig::generateComboStyle() const
{
    QString bg = color("combo", "background", QColor("#3c3c3c")).name();
    QString fg = color("combo", "foreground", QColor("#d4d4d4")).name();
    QString border = color("combo", "border", QColor("#555555")).name();
    QString hover = color("combo", "itemHover", QColor("#094771")).name();

    return QString(
        "QComboBox { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background-color: %1; color: %2;"
        "  selection-background-color: %4; }"
    ).arg(bg, fg, border, hover);
}

QString ThemeConfig::generateCheckboxStyle() const
{
    QString fg = color("checkbox", "foreground", QColor("#d4d4d4")).name();
    return QString(
        "QCheckBox { color: %1; spacing: 4px; }"
        "QCheckBox::indicator { width: 14px; height: 14px; }"
        "QRadioButton { color: %1; spacing: 4px; }"
    ).arg(fg);
}

QString ThemeConfig::generateGroupBoxStyle() const
{
    QString fg = color("groupbox", "foreground", QColor("#d4d4d4")).name();
    QString border = color("groupbox", "border", QColor("#3c3c3c")).name();
    QString bg = color("groupbox", "background", color("window", "background", QColor("#1e1e1e")).name()).name();

    return QString(
        "QGroupBox { color: %1; background-color: %2; border: 1px solid %3;"
        "  border-radius: 4px; margin-top: 8px; padding-top: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"
    ).arg(fg, bg, border);
}

QString ThemeConfig::generateSpinBoxStyle() const
{
    QString bg = color("input", "background", QColor("#3c3c3c")).name();
    QString fg = color("input", "foreground", QColor("#d4d4d4")).name();
    QString border = color("input", "border", QColor("#555555")).name();

    return QString(
        "QSpinBox { background-color: %1; color: %2; border: 1px solid %3;"
        "  border-radius: 4px; padding: 2px 4px; }"
    ).arg(bg, fg, border);
}

QString ThemeConfig::generateScrollBarStyle(const QString& owner, const QString& orientation) const
{
    QString scrollBg = color("scrollbar", "background", QColor("#2d2d2d")).name();
    QString handle = color("scrollbar", "handle", QColor("#555555")).name();
    QString dim = (orientation == "vertical") ? "width" : "height";
    QString dimVal = (orientation == "vertical") ? "10px" : "10px";
    QString minDim = (orientation == "vertical") ? "min-height: 20px" : "min-width: 20px";
    QString addSubDim = (orientation == "vertical") ? "height" : "width";

    return QString(
        "%1 QScrollBar:%2 { background: %3; %4: %5; }"
        "%1 QScrollBar::handle:%2 { background: %6; border-radius: 4px; %7; }"
        "%1 QScrollBar::add-line:%2, %1 QScrollBar::sub-line:%2 { %8: 0; }"
    ).arg(owner, orientation, scrollBg, dim, dimVal, handle, minDim, addSubDim);
}

QString ThemeConfig::toStyleSheet() const
{
    QString bg = color("window", "background", QColor("#1e1e1e")).name();
    QString fg = color("window", "foreground", QColor("#d4d4d4")).name();
    QString windowBorder = color("window", "border", QColor("#3c3c3c")).name();

    QString css;

    // Base window styles
    css += QString(
        "QMainWindow, QWidget { background-color: %1; color: %2; }"
        "QMainWindow::separator { background-color: %3; width: 1px; height: 1px; }"
        "QLabel { color: %2; background: transparent; }"
    ).arg(bg, fg, windowBorder);

    // Component-specific styles
    css += generateMenuStyle();
    css += generateToolbarStyle();
    css += generateStatusBarStyle();
    css += generateSplitterStyle();
    css += generateButtonStyle();
    css += generateInputStyle();
    css += generateTreeStyle();
    css += generateListStyle();
    css += generateTextEditStyle();
    css += generateTabStyle();
    css += generateProgressStyle();
    css += generateComboStyle();
    css += generateCheckboxStyle();
    css += generateGroupBoxStyle();
    css += generateSpinBoxStyle();
    css += generateScrollBarStyle("QTextEdit", "vertical");
    css += generateScrollBarStyle("QTextEdit", "horizontal");
    css += generateScrollBarStyle("QTreeWidget", "vertical");
    css += generateScrollBarStyle("QTreeWidget", "horizontal");
    css += generateScrollBarStyle("QListWidget", "vertical");
    css += generateScrollBarStyle("QListWidget", "horizontal");

    return css;
}

bool ThemeConfig::saveToFile(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;

    QJsonDocument doc(m_rawJson);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

ThemeConfig ThemeConfig::fromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ThemeConfig: Cannot open file:" << filePath;
        return ThemeConfig();
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "ThemeConfig: JSON parse error in" << filePath << ":" << error.errorString();
        return ThemeConfig();
    }

    return ThemeConfig(doc.object());
}

bool ThemeConfig::validateJson(const QJsonObject& json, QString* errorMsg)
{
    if (!json.contains("name") || !json.value("name").isString()) {
        if (errorMsg) *errorMsg = "Missing or invalid 'name' field (string)";
        return false;
    }
    if (json.value("name").toString().trimmed().isEmpty()) {
        if (errorMsg) *errorMsg = "'name' field is empty";
        return false;
    }
    // colors is optional but must be an object if present
    if (json.contains("colors") && !json.value("colors").isObject()) {
        if (errorMsg) *errorMsg = "'colors' must be a JSON object";
        return false;
    }
    return true;
}

// ============================================================================
// ThemeManager 实现
// ============================================================================

ThemeManager::ThemeManager(QObject* parent)
    : QObject(parent)
    , m_fileWatcher(new QFileSystemWatcher(this))
{
    // Determine custom themes directory
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    m_themesDir = dataDir + "/themes";

    connect(m_fileWatcher, &QFileSystemWatcher::fileChanged,
            this, &ThemeManager::onFileChanged);
    connect(m_fileWatcher, &QFileSystemWatcher::directoryChanged,
            this, &ThemeManager::onDirectoryChanged);
}

ThemeManager::~ThemeManager()
{
}

void ThemeManager::initialize()
{
    if (m_initialized) return;

    // 1. Load built-in themes
    loadBuiltinThemes();

    // 2. Scan custom themes directory
    scanCustomThemes();

    // 3. Load saved theme preference
    QString savedTheme = loadThemePreference();
    if (savedTheme.isEmpty() || !m_themes.contains(savedTheme)) {
        savedTheme = "dark"; // Default to dark
    }

    setTheme(savedTheme);
    m_initialized = true;
}

void ThemeManager::loadBuiltinThemes()
{
    // --- System Default (no custom styling) ---
    {
        QJsonObject root;
        root["name"] = QString::fromUtf8("系统默认");
        root["description"] = QString::fromUtf8("使用 Qt 原生默认样式，无自定义着色");

        QJsonObject colors;
        QJsonObject window;
        window["background"] = "";
        window["foreground"] = "";
        colors["window"] = window;
        root["colors"] = colors;

        ThemeConfig t(root);
        m_themes["default"] = t;
        m_themeNames.append("default");
    }

    // --- Dark Theme (VS Code-inspired) ---
    {
        QJsonObject root;
        root["name"] = "Dark";
        root["description"] = QString::fromUtf8("暗色主题，灵感来自 VS Code");

        QJsonObject colors;

        QJsonObject window; window["background"] = "#1e1e1e"; window["foreground"] = "#d4d4d4"; window["border"] = "#3c3c3c";
        colors["window"] = window;

        QJsonObject toolbar; toolbar["background"] = "#2d2d2d"; toolbar["foreground"] = "#d4d4d4"; toolbar["border"] = "#3c3c3c"; toolbar["hover"] = "#3c3c3c"; toolbar["pressed"] = "#094771";
        colors["toolbar"] = toolbar;

        QJsonObject statusbar; statusbar["background"] = "#007acc"; statusbar["foreground"] = "#ffffff";
        colors["statusbar"] = statusbar;

        QJsonObject header; header["background"] = "#2d2d2d";
        colors["header"] = header;

        QJsonObject splitter; splitter["handle"] = "#3c3c3c";
        colors["splitter"] = splitter;

        QJsonObject menubar; menubar["background"] = "#2d2d2d"; menubar["foreground"] = "#d4d4d4"; menubar["border"] = "#3c3c3c"; menubar["hover"] = "#094771";
        colors["menubar"] = menubar;

        QJsonObject menu; menu["background"] = "#2d2d2d"; menu["foreground"] = "#d4d4d4"; menu["border"] = "#3c3c3c"; menu["hover"] = "#094771"; menu["separator"] = "#3c3c3c";
        colors["menu"] = menu;

        QJsonObject button; button["background"] = "#3c3c3c"; button["foreground"] = "#d4d4d4"; button["border"] = "#555555"; button["hover"] = "#4c4c4c"; button["pressed"] = "#094771";
        QJsonObject disabled; disabled["foreground"] = "#666666"; disabled["background"] = "#2d2d2d";
        button["disabled"] = disabled;
        colors["button"] = button;

        QJsonObject input; input["background"] = "#3c3c3c"; input["foreground"] = "#d4d4d4"; input["border"] = "#555555"; input["focusBorder"] = "#1976D2";
        colors["input"] = input;

        QJsonObject tree; tree["background"] = "#1e1e1e"; tree["foreground"] = "#d4d4d4"; tree["alternateBackground"] = "#252525"; tree["border"] = "#3c3c3c"; tree["selected"] = "#094771"; tree["hover"] = "#2a2d2e";
        colors["tree"] = tree;

        QJsonObject list; list["background"] = "#1e1e1e"; list["foreground"] = "#d4d4d4"; list["border"] = "#3c3c3c"; list["selected"] = "#094771"; list["hover"] = "#2a2d2e";
        colors["list"] = list;

        QJsonObject textedit; textedit["background"] = "#1e1e1e"; textedit["foreground"] = "#d4d4d4"; textedit["border"] = "#3c3c3c";
        colors["textedit"] = textedit;

        QJsonObject tab; tab["paneBackground"] = "#1e1e1e"; tab["background"] = "#2d2d2d"; tab["foreground"] = "#d4d4d4"; tab["border"] = "#3c3c3c"; tab["selectedBackground"] = "#1e1e1e"; tab["selectedForeground"] = "#d4d4d4"; tab["hoverBackground"] = "#3c3c3c";
        colors["tab"] = tab;

        QJsonObject progress; progress["background"] = "#2d2d2d"; progress["border"] = "#555555"; progress["foreground"] = "#d4d4d4"; progress["chunk"] = "#1976D2";
        colors["progress"] = progress;

        QJsonObject combo; combo["background"] = "#3c3c3c"; combo["foreground"] = "#d4d4d4"; combo["border"] = "#555555"; combo["itemHover"] = "#094771";
        colors["combo"] = combo;

        QJsonObject checkbox; checkbox["foreground"] = "#d4d4d4";
        colors["checkbox"] = checkbox;

        QJsonObject groupbox; groupbox["foreground"] = "#d4d4d4";
        colors["groupbox"] = groupbox;

        QJsonObject scrollbar; scrollbar["background"] = "#2d2d2d"; scrollbar["handle"] = "#555555";
        colors["scrollbar"] = scrollbar;

        root["colors"] = colors;

        root["trayIcon"] = "";

        ThemeConfig t(root);
        m_themes["dark"] = t;
        m_themeNames.append("dark");
    }

    // --- Light Theme (VS Code-style light) ---
    {
        QJsonObject root;
        root["name"] = "Light";
        root["description"] = QString::fromUtf8("VS Code 风格亮色主题");

        QJsonObject colors;

        QJsonObject window; window["background"] = "#ffffff"; window["foreground"] = "#1f1f1f"; window["border"] = "#e0e0e0";
        colors["window"] = window;

        QJsonObject toolbar; toolbar["background"] = "#f3f3f3"; toolbar["foreground"] = "#1f1f1f"; toolbar["border"] = "#e0e0e0"; toolbar["hover"] = "#e0e0e0"; toolbar["pressed"] = "#cce5ff";
        colors["toolbar"] = toolbar;

        QJsonObject statusbar; statusbar["background"] = "#0066b8"; statusbar["foreground"] = "#ffffff";
        colors["statusbar"] = statusbar;

        QJsonObject header; header["background"] = "#f3f3f3";
        colors["header"] = header;

        QJsonObject splitter; splitter["handle"] = "#e0e0e0";
        colors["splitter"] = splitter;

        QJsonObject menubar; menubar["background"] = "#f3f3f3"; menubar["foreground"] = "#1f1f1f"; menubar["border"] = "#e0e0e0"; menubar["hover"] = "#e8f0fe";
        colors["menubar"] = menubar;

        QJsonObject menu; menu["background"] = "#ffffff"; menu["foreground"] = "#1f1f1f"; menu["border"] = "#d0d0d0"; menu["hover"] = "#e8f0fe"; menu["separator"] = "#e0e0e0";
        colors["menu"] = menu;

        QJsonObject button; button["background"] = "#ffffff"; button["foreground"] = "#1f1f1f"; button["border"] = "#c0c0c0"; button["hover"] = "#e8f0fe"; button["pressed"] = "#cce5ff";
        QJsonObject disabled; disabled["foreground"] = "#aaaaaa"; disabled["background"] = "#f5f5f5";
        button["disabled"] = disabled;
        colors["button"] = button;

        QJsonObject input; input["background"] = "#ffffff"; input["foreground"] = "#1f1f1f"; input["border"] = "#c0c0c0"; input["focusBorder"] = "#0066b8";
        colors["input"] = input;

        QJsonObject tree; tree["background"] = "#ffffff"; tree["foreground"] = "#1f1f1f"; tree["alternateBackground"] = "#f8f8f8"; tree["border"] = "#e0e0e0"; tree["selected"] = "#e8f0fe"; tree["hover"] = "#f0f0f0";
        colors["tree"] = tree;

        QJsonObject list; list["background"] = "#ffffff"; list["foreground"] = "#1f1f1f"; list["border"] = "#e0e0e0"; list["selected"] = "#e8f0fe"; list["hover"] = "#f0f0f0";
        colors["list"] = list;

        QJsonObject textedit; textedit["background"] = "#ffffff"; textedit["foreground"] = "#1f1f1f"; textedit["border"] = "#e0e0e0";
        colors["textedit"] = textedit;

        QJsonObject tab; tab["paneBackground"] = "#ffffff"; tab["background"] = "#ececec"; tab["foreground"] = "#1f1f1f"; tab["border"] = "#e0e0e0"; tab["selectedBackground"] = "#ffffff"; tab["selectedForeground"] = "#0066b8"; tab["hoverBackground"] = "#e8f0fe";
        colors["tab"] = tab;

        QJsonObject progress; progress["background"] = "#f3f3f3"; progress["border"] = "#e0e0e0"; progress["foreground"] = "#1f1f1f"; progress["chunk"] = "#0066b8";
        colors["progress"] = progress;

        QJsonObject combo; combo["background"] = "#ffffff"; combo["foreground"] = "#1f1f1f"; combo["border"] = "#c0c0c0"; combo["itemHover"] = "#e8f0fe";
        colors["combo"] = combo;

        QJsonObject checkbox; checkbox["foreground"] = "#1f1f1f";
        colors["checkbox"] = checkbox;

        QJsonObject groupbox; groupbox["foreground"] = "#1f1f1f"; groupbox["border"] = "#e0e0e0";
        colors["groupbox"] = groupbox;

        QJsonObject scrollbar; scrollbar["background"] = "#f3f3f3"; scrollbar["handle"] = "#c0c0c0";
        colors["scrollbar"] = scrollbar;

        root["colors"] = colors;

        root["trayIcon"] = "";

        ThemeConfig t(root);
        m_themes["light"] = t;
        m_themeNames.append("light");
    }
}

void ThemeManager::scanCustomThemes()
{
    QDir dir(m_themesDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        return;
    }

    // Ensure the themes directory is watched for content changes
    if (!m_fileWatcher->directories().contains(m_themesDir)) {
        m_fileWatcher->addPath(m_themesDir);
    }

    QStringList filters = {"*.json", "*.theme"};
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);
    for (const auto& fi : files) {
        registerTheme(fi.absoluteFilePath());
    }
}

bool ThemeManager::registerTheme(const QString& filePath)
{
    ThemeConfig theme = ThemeConfig::fromFile(filePath);
    if (!theme.isValid()) {
        qWarning() << "ThemeManager: Skipping invalid theme file:" << filePath;
        return false;
    }

    // Use filename (without extension) as key
    QFileInfo fi(filePath);
    QString key = fi.completeBaseName().toLower().replace(' ', '_');
    if (m_themes.contains(key)) {
        // Override built-in themes with custom files
        qDebug() << "ThemeManager: Overriding theme" << key << "from" << filePath;
    }

    m_themes[key] = theme;
    if (!m_themeNames.contains(key))
        m_themeNames.append(key);

    // Watch this file for changes
    m_fileWatcher->addPath(filePath);

    qDebug() << "ThemeManager: Loaded theme" << theme.name << "from" << filePath;
    return true;
}

void ThemeManager::setupFileWatcher()
{
    // themes directory itself is watched; individual files are added in registerTheme()
}

void ThemeManager::onFileChanged(const QString& filePath)
{
    QFileInfo fi(filePath);

    // Windows editors often save by: write temp -> delete original -> rename temp to original
    // This invalidates QFileSystemWatcher's file handle. Handle this case:
    if (!fi.exists()) {
        // File was deleted or replaced (common on Windows editors).
        m_fileWatcher->removePath(filePath);
        // If the file still exists on disk (was replaced), re-register
        if (QFile::exists(filePath)) {
            qDebug() << "ThemeManager: Theme file was replaced (re-watching):" << filePath;
            QFileInfo newFi(filePath);
            QString key = newFi.completeBaseName().toLower().replace(' ', '_');
            registerTheme(filePath);
            if (m_themes.contains(key) && m_currentThemeKey == key) {
                m_currentTheme = m_themes[key];
                emit themeModified(key);
                applyTheme();
            }
        } else {
            qDebug() << "ThemeManager: Theme file removed:" << filePath;
        }
        return;
    }

    qDebug() << "ThemeManager: Theme file changed:" << filePath;

    // Re-register theme
    QString key = fi.completeBaseName().toLower().replace(' ', '_');
    if (m_themes.contains(key)) {
        registerTheme(filePath);
        // If the changed theme is the current one, re-apply
        if (m_currentThemeKey == key) {
            emit themeModified(key);
            m_currentTheme = m_themes[key];
            applyTheme();
        }
    }
}

void ThemeManager::onDirectoryChanged(const QString& dirPath)
{
    Q_UNUSED(dirPath);
    qDebug() << "ThemeManager: Themes directory changed, rescanning...";

    // Remember current theme key before rescan
    QString currentKey = m_currentThemeKey;

    rescanThemes();

    // If current theme was a custom one, re-apply
    if (!currentKey.isEmpty() && m_themes.contains(currentKey)) {
        m_currentTheme = m_themes[currentKey];
        applyTheme();
    }
}

bool ThemeManager::setTheme(const QString& name)
{
    if (!m_themes.contains(name)) {
        qWarning() << "ThemeManager: Theme not found:" << name;
        return false;
    }

    m_currentThemeKey = name;
    m_currentTheme = m_themes[name];
    saveThemePreference(name);
    applyTheme();
    emit themeChanged(name);
    return true;
}

void ThemeManager::applyTheme()
{
    if (!m_currentTheme.isValid()) return;
    QApplication* app = qobject_cast<QApplication*>(QCoreApplication::instance());
    if (!app) return; // No QApplication yet (testing context)

    // Apply application font
    QFont appFont = m_currentTheme.applicationFont();
    app->setFont(appFont);

    // Apply stylesheet
    QString sheet = m_currentTheme.toStyleSheet();
    app->setStyleSheet(sheet);

    qDebug() << "ThemeManager: Applied theme:" << m_currentTheme.name;
}

void ThemeManager::rescanThemes()
{
    // Clear watch paths
    QStringList watched = m_fileWatcher->files();
    if (!watched.isEmpty()) m_fileWatcher->removePaths(watched);

    // Remove non-builtin theme names
    QStringList builtins = {"default", "dark", "light"};
    m_themeNames = builtins;

    // Re-scan
    scanCustomThemes();
}

ThemeConfig* ThemeManager::theme(const QString& name)
{
    auto it = m_themes.find(name);
    if (it != m_themes.end())
        return &it.value();
    return nullptr;
}

void ThemeManager::saveThemePreference(const QString& name)
{
    QSettings settings;
    settings.setValue("app/theme", name);
}

QString ThemeManager::loadThemePreference()
{
    QSettings settings;
    return settings.value("app/theme", "dark").toString();
}
