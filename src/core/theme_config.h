/*
 * theme_config.h - AnyTXT Searcher 主题配置系统
 *
 * 功能说明：基于 JSON 的主题配置系统，支持通过配置文件自定义
 *          界面着色、字体、点击效果及托盘图标。
 *          支持配置文件热加载（通过 ThemeManager 文件监控实现）。
 *
 * 用法：
 *   1. 程序启动时自动加载 themes/ 目录下的所有 .json 主题文件
 *   2. 内置三个主题：系统默认 (default)、暗色 (dark)、亮色 (light)
 *   3. 用户可编辑 JSON 文件自定义主题，修改后自动生效
 *   4. 通过 QSettings 保存用户选中的主题名
 *
 * JSON 主题文件结构：
 *   {
 *     "name": "主题名称",
 *     "description": "主题描述（可选）",
 *     "trayIcon": "路径/图标文件名（可选）",
 *     "colors": { ... },
 *     "fonts": { ... },
 *     "effects": { ... }
 *   }
 */

#ifndef ANYTXT_THEME_CONFIG_H
#define ANYTXT_THEME_CONFIG_H

#include <QString>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QColor>
#include <QFont>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QSet>

/**
 * @brief 主题配置数据结构
 *
 * 存储单套主题的完整配置，包括颜色、字体和特效参数。
 * 提供从 JSON 对象解析和生成 QSS 样式表的方法。
 */
class ThemeConfig {
public:
    ThemeConfig();
    explicit ThemeConfig(const QJsonObject& json);

    /** @brief 主题元信息 */
    QString name;             ///< 主题名称（用于显示和标识）
    QString description;      ///< 主题描述（可选）
    QString trayIconPath;     ///< 托盘图标路径（空则使用默认图标）

    /** @brief 从 JSON 对象解析主题 */
    void parse(const QJsonObject& json);

    /** @brief 生成完整 QSS 样式表字符串 */
    QString toStyleSheet() const;

    /** @brief 获取配置的应用字体 */
    QFont applicationFont() const;
    QFont codeFont() const;

    /** @brief 验证主题有效性 */
    bool isValid() const { return !name.isEmpty(); }

    /** @brief 获取颜色值（带默认值） */
    QColor color(const QString& category, const QString& key,
                 const QColor& defaultVal = QColor()) const;

    /** @brief 获取字体对象 */
    QFont font(const QString& key, const QFont& defaultVal = QFont()) const;

    /** @brief 获取效果配置 */
    QString effect(const QString& key, const QString& defaultVal = QString()) const;

    /** @brief 获取托盘图标完整路径（相对于主题文件所在目录解析） */
    QString resolvedTrayIconPath(const QString& themeDir) const;

    /** @brief 保存主题到 JSON 文件 */
    bool saveToFile(const QString& filePath) const;

    /** @brief 从 JSON 文件加载主题 */
    static ThemeConfig fromFile(const QString& filePath);

    /** @brief 校验 JSON 是否为主题有效格式 */
    static bool validateJson(const QJsonObject& json, QString* errorMsg = nullptr);

private:
    QJsonObject m_rawJson;    ///< 原始 JSON 数据（保留以支持未识别字段）
    QJsonObject m_colors;     ///< colors 部分
    QJsonObject m_fonts;      ///< fonts 部分
    QJsonObject m_effects;    ///< effects 部分

    /** @brief 从嵌套路径获取字符串值，如 "button.background" */
    QString resolveNested(const QJsonObject& root, const QString& dottedPath,
                          const QString& defaultVal = QString()) const;

    /** @brief 生成具体控件类型的 QSS 片段 */
    QString generateButtonStyle() const;
    QString generateToolbarStyle() const;
    QString generateStatusBarStyle() const;
    QString generateSplitterStyle() const;
    QString generateMenuStyle() const;
    QString generateInputStyle() const;
    QString generateTreeStyle() const;
    QString generateListStyle() const;
    QString generateTextEditStyle() const;
    QString generateTabStyle() const;
    QString generateProgressStyle() const;
    QString generateComboStyle() const;
    QString generateCheckboxStyle() const;
    QString generateGroupBoxStyle() const;
    QString generateSpinBoxStyle() const;
    QString generateScrollBarStyle(const QString& owner, const QString& orientation) const;
};

/**
 * @brief 主题管理器
 *
 * 管理主题集合，提供主题加载、切换、应用功能。
 * 支持通过文件系统监控自动重载主题配置。
 */
class ThemeManager : public QObject {
    Q_OBJECT
public:
    explicit ThemeManager(QObject* parent = nullptr);
    ~ThemeManager() override;

    /** @brief 初始化：加载内置主题和自定义主题 */
    void initialize();

    /** @brief 获取当前主题 */
    const ThemeConfig& currentTheme() const { return m_currentTheme; }

    /** @brief 获取当前主题名（映射键，如 "dark"） */
    QString currentThemeName() const { return m_currentThemeKey; }

    /** @brief 获取可用主题列表 */
    QStringList availableThemes() const { return m_themeNames; }

    /** @brief 设置主题（通过名称） */
    bool setTheme(const QString& name);

    /** @brief 应用当前主题到整个应用 */
    void applyTheme();

    /** @brief 获取主题配置 */
    ThemeConfig* theme(const QString& name);

    /** @brief 获取主题目录路径 */
    QString themesDirectory() const { return m_themesDir; }

    /** @brief 重新扫描主题目录 */
    void rescanThemes();

signals:
    /** @brief 主题已改变（外部监听用） */
    void themeChanged(const QString& themeName);

    /** @brief 主题文件已改动（需要重载） */
    void themeModified(const QString& themeName);

private slots:
    void onFileChanged(const QString& filePath);
    void onDirectoryChanged(const QString& dirPath);

private:
    /** @brief 加载内置主题 */
    void loadBuiltinThemes();

    /** @brief 扫描自定义主题目录 */
    void scanCustomThemes();

    /** @brief 从 JSON 加载并注册主题 */
    bool registerTheme(const QString& filePath);

    /** @brief 设置文件监控 */
    void setupFileWatcher();

    /** @brief 保存用户主题选择 */
    void saveThemePreference(const QString& name);

    /** @brief 加载用户主题选择 */
    QString loadThemePreference();

    QMap<QString, ThemeConfig> m_themes;    ///< 所有可用主题
    QStringList m_themeNames;               ///< 主题名列表（有序）
    ThemeConfig m_currentTheme;             ///< 当前激活的主题
    QString m_themesDir;                    ///< 自定义主题目录
    QFileSystemWatcher* m_fileWatcher;      ///< 文件变更监控
    QString m_currentThemeKey;              ///< 当前主题的映射键
    bool m_initialized = false;
};

#endif // ANYTXT_THEME_CONFIG_H
