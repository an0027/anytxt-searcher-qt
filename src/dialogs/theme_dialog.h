/*
 * theme_dialog.h - 主题配置对话框
 *
 * 功能说明：
 *   可视化主题切换与配置对话框。
 *   - 左侧列表展示所有可用主题（内置+自定义）
 *   - 选中即实时应用，可立即看到效果
 *   - 颜色预览区显示主题关键颜色
 *   - 支持直接打开 JSON 文件编辑
 *   - 支持从当前主题另存为新主题
 */

#ifndef ANYTXT_THEME_DIALOG_H
#define ANYTXT_THEME_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QGridLayout>
#include "core/theme_config.h"

class ThemeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ThemeDialog(ThemeManager* themeMgr, QWidget* parent = nullptr);
    ~ThemeDialog() override = default;

    /** @brief 获取对话框最终选中的主题名 */
    QString selectedTheme() const { return m_selectedTheme; }

private slots:
    void onThemeSelectionChanged();
    void onOpenEditor();
    void onNewTheme();

private:
    void setupUI();
    void populateThemeList();
    void updatePreview(const ThemeConfig& theme);
    void refreshColorPreview(const ThemeConfig& theme);
    QListWidgetItem* createThemeItem(const QString& key, const ThemeConfig& theme);

    ThemeManager* m_themeMgr;
    QString m_selectedTheme;
    QStringList m_themeKeys;   // 有序的主题键名列表

    // UI elements
    QListWidget* m_themeList;
    QLabel* m_nameLabel;
    QLabel* m_descLabel;
    QWidget* m_previewContainer;
    QLabel* m_prevColor[10];   // 最多 10 个颜色预览块
    QLabel* m_prevLabel[10];   // 对应的颜色名称标签
    int m_colorCount;

    // Theme key -> display name mapping from ThemeManager
    QMap<QString, QString> m_keyToDisplay;
    QMap<QString, QString> m_displayToKey;
};

#endif // ANYTXT_THEME_DIALOG_H
