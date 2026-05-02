/*
 * theme_dialog.cpp - 主题配置对话框实现
 *
 * 布局：
 *   ┌─────────────────────────────────────┐
 *   │  主题配置                       ✕   │
 *   ├──────────┬──────────────────────────┤
 *   │ 主题列表  │  [主题名称]              │
 *   │ (QList)  │  [描述]                  │
 *   │          │                          │
 *   │          │  ── 颜色预览 ──           │
 *   │          │  ■■ 背景   ■■ 前景       │
 *   │          │  ■■ 按钮   ■■ 工具栏     │
 *   │          │  ■■ 输入   ■■ 状态栏     │
 *   │          │  ■■ 选中   ■■ 焦点       │
 *   ├──────────┴──────────────────────────┤
 *   │ [打开编辑器] [新建主题]  [确定][取消] │
 *   └─────────────────────────────────────┘
 *
 * 特性：
 *   - 选中主题即实时应用
 *   - 颜色预览块动态更新
 *   - 打开编辑器直接修改 JSON
 *   - 从当前配置另存为新主题
 */

#include "theme_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QApplication>
#include <QFrame>

// ── 颜色预览项定义：颜色键路径 → 显示名称 ──
struct ColorPreviewItem {
    const char* displayName;  // 中文显示名
    const char* category;     // colors 中的大类
    const char* colorKey;     // 颜色键名
};

static const ColorPreviewItem s_colorItems[] = {
    {"窗口背景",   "window",    "background"},
    {"窗口前景",   "window",    "foreground"},
    {"按钮背景",   "button",    "background"},
    {"按钮悬停",   "button",    "hover"},
    {"按钮按下",   "button",    "pressed"},
    {"工具栏",     "toolbar",   "background"},
    {"输入框",     "input",     "background"},
    {"输入焦点",   "input",     "focusBorder"},
    {"树表选中",   "tree",      "selected"},
    {"状态栏",     "statusbar", "background"},
};

static const int s_colorItemCount = sizeof(s_colorItems) / sizeof(s_colorItems[0]);

// ============================================================================
// ThemeDialog
// ============================================================================

ThemeDialog::ThemeDialog(ThemeManager* themeMgr, QWidget* parent)
    : QDialog(parent)
    , m_themeMgr(themeMgr)
{
    setWindowTitle(tr("主题配置"));
    resize(620, 460);
    setMinimumSize(540, 380);

    setupUI();
    populateThemeList();

    // Select current theme
    QString currentKey = themeMgr->currentThemeName();
    for (int i = 0; i < m_themeKeys.size(); ++i) {
        if (m_themeKeys[i] == currentKey) {
            m_themeList->setCurrentRow(i);
            break;
        }
    }
}

void ThemeDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ─── Left: Theme list ───
    m_themeList = new QListWidget(this);
    m_themeList->setMinimumWidth(160);
    m_themeList->setMaximumWidth(220);
    m_themeList->setSpacing(2);
    m_themeList->setStyleSheet(
        "QListWidget { border: 1px solid palette(mid); border-radius: 4px; }"
        "QListWidget::item { padding: 8px 12px; border-radius: 3px; }"
        "QListWidget::item:selected { font-weight: bold; }"
    );
    connect(m_themeList, &QListWidget::currentItemChanged,
            this, &ThemeDialog::onThemeSelectionChanged);
    splitter->addWidget(m_themeList);

    // ─── Right: Preview panel ───
    auto* rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(8, 0, 0, 0);
    rightLayout->setSpacing(6);

    // Theme info
    m_nameLabel = new QLabel(this);
    m_nameLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    m_descLabel = new QLabel(this);
    m_descLabel->setStyleSheet("color: palette(text); font-size: 12px;");
    m_descLabel->setWordWrap(true);

    rightLayout->addWidget(m_nameLabel);
    rightLayout->addWidget(m_descLabel);

    // Separator
    auto* sep = new QFrame(this);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    rightLayout->addWidget(sep);

    // Color preview
    auto* previewGroup = new QGroupBox(tr("颜色预览"), this);
    auto* previewGrid = new QGridLayout(previewGroup);
    previewGrid->setSpacing(6);

    m_colorCount = s_colorItemCount;
    for (int i = 0; i < m_colorCount; ++i) {
        // Color swatch
        m_prevColor[i] = new QLabel(this);
        m_prevColor[i]->setFixedSize(28, 20);
        m_prevColor[i]->setStyleSheet(
            "border: 1px solid #888; border-radius: 3px; background-color: #ccc;");
        previewGrid->addWidget(m_prevColor[i], i, 0, Qt::AlignLeft);

        // Label
        m_prevLabel[i] = new QLabel(s_colorItems[i].displayName, this);
        m_prevLabel[i]->setStyleSheet("font-size: 12px;");
        previewGrid->addWidget(m_prevLabel[i], i, 1, Qt::AlignLeft);

        // Color hex value
        auto* hexLabel = new QLabel(this);
        hexLabel->setObjectName("hex_" + QString::number(i));
        hexLabel->setStyleSheet("color: #888; font-size: 11px; font-family: Consolas;");
        hexLabel->setText("");
        previewGrid->addWidget(hexLabel, i, 2, Qt::AlignLeft);
    }
    previewGrid->setColumnStretch(3, 1);

    m_previewContainer = previewGroup;
    rightLayout->addWidget(previewGroup, 1);
    rightLayout->addStretch();

    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({180, 380});

    mainLayout->addWidget(splitter, 1);

    // ─── Bottom buttons ───
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);

    auto* editBtn = new QPushButton(tr("打开编辑器编辑"), this);
    editBtn->setToolTip(tr("使用系统默认编辑器打开当前主题的 JSON 配置文件"));
    connect(editBtn, &QPushButton::clicked, this, &ThemeDialog::onOpenEditor);
    bottomLayout->addWidget(editBtn);

    auto* newBtn = new QPushButton(tr("新建主题"), this);
    newBtn->setToolTip(tr("从当前主题另存为新主题"));
    connect(newBtn, &QPushButton::clicked, this, &ThemeDialog::onNewTheme);
    bottomLayout->addWidget(newBtn);

    bottomLayout->addStretch();

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    bottomLayout->addWidget(btnBox);

    mainLayout->addLayout(bottomLayout);
}

void ThemeDialog::populateThemeList()
{
    m_themeKeys = m_themeMgr->availableThemes();
    m_themeList->clear();
    m_keyToDisplay.clear();
    m_displayToKey.clear();

    for (const auto& key : m_themeKeys) {
        ThemeConfig* tc = m_themeMgr->theme(key);
        if (!tc) continue;

        QString displayName = tc->name;
        if (displayName.isEmpty()) displayName = key;

        m_keyToDisplay[key] = displayName;
        m_displayToKey[displayName] = key;

        auto* item = new QListWidgetItem(displayName, m_themeList);
        item->setData(Qt::UserRole, key);

        // Add a small badge for built-in vs custom
        if (key == "default" || key == "dark" || key == "light") {
            item->setToolTip(tr("内置主题"));
        } else {
            item->setToolTip(tr("自定义主题: %1").arg(key));
        }
    }
}

void ThemeDialog::onThemeSelectionChanged()
{
    QListWidgetItem* item = m_themeList->currentItem();
    if (!item) return;

    QString key = item->data(Qt::UserRole).toString();
    ThemeConfig* tc = m_themeMgr->theme(key);
    if (!tc) return;

    m_selectedTheme = key;

    // Update info
    m_nameLabel->setText(tc->name);
    m_descLabel->setText(tc->description.isEmpty() ? tr("(无描述)") : tc->description);

    // Update color preview
    updatePreview(*tc);

    // Apply immediately so user sees the effect
    m_themeMgr->setTheme(key);
}

void ThemeDialog::updatePreview(const ThemeConfig& theme)
{
    refreshColorPreview(theme);
}

void ThemeDialog::refreshColorPreview(const ThemeConfig& theme)
{
    for (int i = 0; i < m_colorCount && i < s_colorItemCount; ++i) {
        const auto& ci = s_colorItems[i];
        QColor c = theme.color(ci.category, ci.colorKey);

        QString bgColor = c.isValid() ? c.name() : "#cccccc";
        m_prevColor[i]->setStyleSheet(
            QString("border: 1px solid #888; border-radius: 3px; background-color: %1;")
                .arg(bgColor));

        // Update hex label
        auto* hexLabel = findChild<QLabel*>("hex_" + QString::number(i));
        if (hexLabel) {
            hexLabel->setText(c.isValid() ? c.name() : "");
        }
    }
}

QListWidgetItem* ThemeDialog::createThemeItem(const QString& key, const ThemeConfig& theme)
{
    Q_UNUSED(key);
    auto* item = new QListWidgetItem(theme.name);
    return item;
}

void ThemeDialog::onOpenEditor()
{
    // Find the current theme's file path (if custom)
    QString key = m_selectedTheme;
    if (key.isEmpty()) return;

    // Built-in themes have no file; open the themes directory instead
    QStringList builtins = {"default", "dark", "light"};
    if (builtins.contains(key)) {
        // Open the themes directory
        QString dir = m_themeMgr->themesDirectory();
        if (QDir(dir).exists()) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        } else {
            QMessageBox::information(this, tr("提示"),
                tr("内置主题无法直接编辑。\n新建一个自定义主题或打开 themes 目录查看示例。"));
        }
        return;
    }

    // For custom themes, try to find the file in themes directory
    QString dir = m_themeMgr->themesDirectory();
    QString filePath = dir + "/" + key + ".json";
    if (QFile::exists(filePath)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
    } else {
        // Try .theme extension
        filePath = dir + "/" + key + ".theme";
        if (QFile::exists(filePath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(filePath));
        } else {
            QMessageBox::warning(this, tr("提示"),
                tr("找不到主题文件: %1").arg(filePath));
        }
    }
}

void ThemeDialog::onNewTheme()
{
    // Get current theme config as base
    QString currentKey = m_themeMgr->currentThemeName();
    ThemeConfig* base = m_themeMgr->theme(currentKey);
    if (!base) return;

    // Ask for new theme name (file name)
    bool ok;
    QString newName = QInputDialog::getText(this,
        tr("新建主题"),
        tr("输入新主题名称（将作为文件名）："),
        QLineEdit::Normal,
        tr("我的主题"), &ok);
    if (!ok || newName.trimmed().isEmpty()) return;

    // Sanitize filename
    QString safeName = newName.trimmed();
    safeName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");

    // Ask for display name
    QString displayName = QInputDialog::getText(this,
        tr("主题显示名称"),
        tr("输入主题显示名称："),
        QLineEdit::Normal,
        safeName, &ok);
    if (!ok || displayName.trimmed().isEmpty()) return;

    // Create the JSON file
    QString dir = m_themeMgr->themesDirectory();
    QString filePath = dir + "/" + safeName + ".json";

    if (QFile::exists(filePath)) {
        auto ret = QMessageBox::question(this, tr("文件已存在"),
            tr("主题文件 '%1.json' 已存在，是否覆盖？").arg(safeName),
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    // We need to serialize the current theme. Since ThemeConfig doesn't have
    // a toJson() method directly, we re-create a minimal JSON from the theme data.
    // A better approach: save from the file watcher's perspective.
    // For now, create a JSON with the current theme's colors.
    QJsonObject root;
    root["name"] = displayName.trimmed();
    root["description"] = tr("从 %1 创建的自定义主题").arg(base->name);

    QJsonObject colors;

    // Build colors from the ThemeConfig by accessing raw data
    // Use the built-in color structure
    struct ColorDef {
        const char* category;
        const char* key;
    };
    ColorDef colorDefs[] = {
        {"window", "background"}, {"window", "foreground"}, {"window", "border"},
        {"menubar", "background"}, {"menubar", "foreground"}, {"menubar", "hover"},
        {"menu", "background"}, {"menu", "foreground"}, {"menu", "hover"}, {"menu", "separator"},
        {"toolbar", "background"}, {"toolbar", "foreground"}, {"toolbar", "border"},
        {"toolbar", "hover"}, {"toolbar", "pressed"},
        {"statusbar", "background"}, {"statusbar", "foreground"},
        {"button", "background"}, {"button", "foreground"}, {"button", "border"},
        {"button", "hover"}, {"button", "pressed"},
        {"input", "background"}, {"input", "foreground"}, {"input", "border"}, {"input", "focusBorder"},
        {"tree", "background"}, {"tree", "foreground"}, {"tree", "border"},
        {"tree", "selected"}, {"tree", "hover"}, {"tree", "alternateBackground"},
        {"list", "background"}, {"list", "foreground"}, {"list", "border"},
        {"list", "selected"}, {"list", "hover"},
        {"textedit", "background"}, {"textedit", "foreground"}, {"textedit", "border"},
        {"tab", "paneBackground"}, {"tab", "background"}, {"tab", "foreground"}, {"tab", "border"},
        {"tab", "selectedBackground"}, {"tab", "selectedForeground"}, {"tab", "hoverBackground"},
        {"progress", "background"}, {"progress", "border"}, {"progress", "foreground"}, {"progress", "chunk"},
        {"combo", "background"}, {"combo", "foreground"}, {"combo", "border"}, {"combo", "itemHover"},
        {"checkbox", "foreground"},
        {"groupbox", "foreground"}, {"groupbox", "border"},
        {"scrollbar", "background"}, {"scrollbar", "handle"},
        {"splitter", "handle"},
        {"header", "background"},
    };

    // Build nested JSON objects for colors
    QMap<QString, QJsonObject> catMap;
    for (const auto& cd : colorDefs) {
        QString cat = cd.category;
        QString k = cd.key;
        // Get the default color for this category/key from the current theme
        // Since we're reading from the ThemeConfig, we need default values
        QColor defaultValue;
        if (k == "foreground" || k == "background" || k == "border" ||
            k == "handle" || k == "separator" || k == "chunk" ||
            k == "hoverBackground" || k == "alternateBackground" || k == "itemHover") {
            defaultValue = QColor("#d4d4d4");
        }
        QColor val = base->color(cat, k, defaultValue);
        if (val.isValid()) {
            if (!catMap.contains(cat)) catMap[cat] = QJsonObject();
            QJsonObject obj = catMap[cat];
            obj[k] = val.name();
            catMap[cat] = obj;
        }
    }

    // Add button disabled state
    QColor disabledFg = base->color("button", "disabled.foreground", QColor("#666666"));
    QColor disabledBg = base->color("button", "disabled.background", QColor("#2d2d2d"));
    QJsonObject disabled;
    disabled["foreground"] = disabledFg.name();
    disabled["background"] = disabledBg.name();
    if (catMap.contains("button")) {
        QJsonObject btn = catMap["button"];
        btn["disabled"] = disabled;
        catMap["button"] = btn;
    }

    for (auto it = catMap.begin(); it != catMap.end(); ++it) {
        colors[it.key()] = it.value();
    }
    root["colors"] = colors;

    // Write file
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QJsonDocument doc(root);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();

        // The directory watcher will pick this up and add it to theme list
        QMessageBox::information(this, tr("成功"),
            tr("新主题已创建: %1\n点击\"确定\"后将自动切换到该主题。").arg(filePath));

        // Switch to the new theme immediately
        QString newKey = safeName.toLower();
        // Wait a moment for file watcher to pick it up, then force register
        m_themeMgr->rescanThemes();
        m_themeMgr->setTheme(newKey);

        // Refresh the list and select new theme
        populateThemeList();
        for (int i = 0; i < m_themeKeys.size(); ++i) {
            if (m_themeKeys[i] == newKey) {
                m_themeList->setCurrentRow(i);
                break;
            }
        }
    } else {
        QMessageBox::warning(this, tr("错误"),
            tr("无法创建主题文件: %1").arg(filePath));
    }
}
