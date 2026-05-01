/*
 * watch_settings_dialog.h - 文件监听设置对话框

功能说明：配置文件系统监听的对话框，包括监听开关、
监听文件夹管理、扫描间隔设置。
 */

#ifndef ANYTXT_WATCH_SETTINGS_DIALOG_H
#define ANYTXT_WATCH_SETTINGS_DIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QSpinBox>
#include <QVBoxLayout>

class IndexConfig;

class WatchSettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit WatchSettingsDialog(const QStringList& currentFolders,
                                 bool watching,
                                 int intervalMs,
                                 QWidget* parent = nullptr);
    ~WatchSettingsDialog() override = default;

    QStringList watchedFolders() const { return m_folders; }
    bool isWatchingEnabled() const;
    int watchInterval() const;

private slots:
    void onAddFolder();
    void onRemoveFolder();

private:
    void setupUI();

    QListWidget* m_folderList;
    QPushButton* m_addBtn;
    QPushButton* m_removeBtn;
    QCheckBox* m_enableWatchCheck;
    QSpinBox* m_intervalSpin;
    QStringList m_folders;
};

#endif // ANYTXT_WATCH_SETTINGS_DIALOG_H
