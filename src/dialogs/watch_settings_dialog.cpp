/*
 * watch_settings_dialog.cpp - 文件监听设置实现

实现监听设置对话框的 UI 布局和交互逻辑。
 */

#include "watch_settings_dialog.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>

WatchSettingsDialog::WatchSettingsDialog(const QStringList& currentFolders,
                                         bool watching,
                                         int intervalMs,
                                         QWidget* parent)
    : QDialog(parent), m_folders(currentFolders)
{
    setWindowTitle("智能索引设置");
    setMinimumSize(500, 400);
    setupUI();

    m_enableWatchCheck->setChecked(watching);
    m_intervalSpin->setValue(intervalMs / 1000);
}

void WatchSettingsDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Enable/disable
    m_enableWatchCheck = new QCheckBox("启用文件系统监听");
    m_enableWatchCheck->setToolTip("自动检测文件夹中的新增和修改文件");
    mainLayout->addWidget(m_enableWatchCheck);

    // Interval
    auto* intervalLayout = new QHBoxLayout();
    intervalLayout->addWidget(new QLabel("扫描间隔(秒):"));
    m_intervalSpin = new QSpinBox();
    m_intervalSpin->setRange(1, 60);
    m_intervalSpin->setValue(2);
    m_intervalSpin->setSuffix(" 秒");
    intervalLayout->addWidget(m_intervalSpin);
    intervalLayout->addStretch();
    mainLayout->addLayout(intervalLayout);

    // Folder list
    auto* folderGroup = new QGroupBox("监听文件夹");
    auto* folderLayout = new QVBoxLayout(folderGroup);

    m_folderList = new QListWidget();
    for (const auto& folder : m_folders) {
        m_folderList->addItem(folder);
    }
    folderLayout->addWidget(m_folderList);

    auto* btnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton("添加文件夹...");
    m_removeBtn = new QPushButton("移除");
    connect(m_addBtn, &QPushButton::clicked, this, &WatchSettingsDialog::onAddFolder);
    connect(m_removeBtn, &QPushButton::clicked, this, &WatchSettingsDialog::onRemoveFolder);
    btnLayout->addWidget(m_addBtn);
    btnLayout->addWidget(m_removeBtn);
    btnLayout->addStretch();
    folderLayout->addLayout(btnLayout);

    mainLayout->addWidget(folderGroup);

    mainLayout->addStretch();

    // OK/Cancel
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        // Validate
        if (m_enableWatchCheck->isChecked() && m_folderList->count() == 0) {
            QMessageBox::warning(this, "提示", "启用监听后至少需要一个监听文件夹");
            return;
        }
        m_folders.clear();
        for (int i = 0; i < m_folderList->count(); i++) {
            m_folders.append(m_folderList->item(i)->text());
        }
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttons);
}

bool WatchSettingsDialog::isWatchingEnabled() const
{
    return m_enableWatchCheck->isChecked();
}

int WatchSettingsDialog::watchInterval() const
{
    return m_intervalSpin->value() * 1000;
}

void WatchSettingsDialog::onAddFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "选择监听文件夹");
    if (!dir.isEmpty()) {
        // Check for duplicates
        for (int i = 0; i < m_folderList->count(); i++) {
            if (m_folderList->item(i)->text() == dir) {
                QMessageBox::information(this, "提示", "该文件夹已在列表中");
                return;
            }
        }
        m_folderList->addItem(dir);
    }
}

void WatchSettingsDialog::onRemoveFolder()
{
    auto* item = m_folderList->currentItem();
    if (item) {
        delete m_folderList->takeItem(m_folderList->row(item));
    }
}
