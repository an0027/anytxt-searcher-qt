/*
 * preferences_dialog.cpp - 偏好设置对话框实现
 */

#include "preferences_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QColorDialog>
#include <QSettings>
#include <QFont>
#include <QComboBox>

PreferencesDialog::PreferencesDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("偏好设置"));
    resize(480, 400);
    setMinimumSize(420, 360);

    setupUI();
    loadCurrentSettings();
}

void PreferencesDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    m_tabWidget = new QTabWidget(this);
    m_tabWidget->addTab(createGeneralTab(), tr("常规"));
    m_tabWidget->addTab(createDisplayTab(), tr("显示"));
    m_tabWidget->addTab(createIndexTab(), tr("索引"));
    layout->addWidget(m_tabWidget, 1);

    auto* btnBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btnBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::onApply);
    connect(btnBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(btnBox);
}

QWidget* PreferencesDialog::createGeneralTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    auto* searchGroup = new QGroupBox(tr("搜索"), w);
    auto* searchForm = new QFormLayout(searchGroup);

    m_scopeCombo = new QComboBox(w);
    m_scopeCombo->addItem(tr("全文"), "all");
    m_scopeCombo->addItem(tr("文件名"), "file");
    m_scopeCombo->addItem(tr("标题"), "title");
    searchForm->addRow(tr("默认搜索范围:"), m_scopeCombo);

    m_pageSizeSpin = new QSpinBox(w);
    m_pageSizeSpin->setRange(10, 500);
    m_pageSizeSpin->setSingleStep(10);
    m_pageSizeSpin->setSuffix(tr(" 条"));
    searchForm->addRow(tr("每页结果数:"), m_pageSizeSpin);

    m_autoLoadCheck = new QCheckBox(tr("启动时自动加载上次搜索"), w);
    searchForm->addRow(m_autoLoadCheck);

    layout->addWidget(searchGroup);
    layout->addStretch();
    return w;
}

QWidget* PreferencesDialog::createDisplayTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    auto* themeGroup = new QGroupBox(tr("主题"), w);
    auto* themeForm = new QFormLayout(themeGroup);

    m_themeCombo = new QComboBox(w);
    m_themeCombo->addItem(tr("系统默认"), 0);
    m_themeCombo->addItem(tr("暗色主题"), 1);
    m_themeCombo->addItem(tr("VS Code 亮色"), 2);
    themeForm->addRow(tr("界面主题:"), m_themeCombo);

    layout->addWidget(themeGroup);

    auto* highlightGroup = new QGroupBox(tr("高亮"), w);
    auto* highlightForm = new QFormLayout(highlightGroup);

    m_colorBtn = new QPushButton(w);
    m_colorBtn->setFixedSize(60, 28);
    m_colorBtn->setStyleSheet("background-color: #FFD54F; border: 1px solid #888; border-radius: 3px;");
    connect(m_colorBtn, &QPushButton::clicked, this, &PreferencesDialog::onPickColor);

    auto* colorRow = new QHBoxLayout();
    colorRow->addWidget(m_colorBtn);
    colorRow->addWidget(new QLabel(tr("点击选择颜色"), w));
    highlightForm->addRow(tr("搜索高亮颜色:"), colorRow);

    layout->addWidget(highlightGroup);
    layout->addStretch();
    return w;
}

QWidget* PreferencesDialog::createIndexTab()
{
    auto* w = new QWidget(this);
    auto* layout = new QVBoxLayout(w);

    auto* indexGroup = new QGroupBox(tr("索引选项"), w);
    auto* indexForm = new QFormLayout(indexGroup);

    m_batchSizeSpin = new QSpinBox(w);
    m_batchSizeSpin->setRange(10, 1000);
    m_batchSizeSpin->setSingleStep(10);
    m_batchSizeSpin->setSuffix(tr(" 条"));
    m_batchSizeSpin->setToolTip(tr("批量索引时每批处理的文档数，数值越大概率越快但内存占用更高"));
    indexForm->addRow(tr("批量大小:"), m_batchSizeSpin);

    m_spellingCheck = new QCheckBox(tr("启用拼写检查建议"), w);
    m_spellingCheck->setToolTip(tr("搜索不到结果时，根据相似拼写给出建议"));
    indexForm->addRow(m_spellingCheck);

    layout->addWidget(indexGroup);
    layout->addStretch();
    return w;
}

void PreferencesDialog::loadCurrentSettings()
{
    QSettings settings;

    int scopeIdx = m_scopeCombo->findData(settings.value("search/lastScope", "all").toString());
    if (scopeIdx >= 0) m_scopeCombo->setCurrentIndex(scopeIdx);

    m_pageSizeSpin->setValue(settings.value("search/pageSize", 50).toInt());
    m_autoLoadCheck->setChecked(settings.value("search/autoLoadLastSearch", false).toBool());
    int themeIdx = m_themeCombo->findData(settings.value("app/themeMode", 0).toInt());
    if (themeIdx >= 0) m_themeCombo->setCurrentIndex(themeIdx);

    m_selectedColor = QColor(settings.value("app/highlightColor", "#FFD54F").toString());
    m_colorBtn->setStyleSheet(
        QString("background-color: %1; border: 1px solid #888; border-radius: 3px;")
            .arg(m_selectedColor.name()));

    m_batchSizeSpin->setValue(settings.value("index/batchSize", 100).toInt());
    m_spellingCheck->setChecked(settings.value("index/enableSpelling", false).toBool());
}

void PreferencesDialog::onApply()
{
    QSettings settings;
    settings.setValue("search/lastScope", m_scopeCombo->currentData().toString());
    settings.setValue("search/pageSize", m_pageSizeSpin->value());
    settings.setValue("search/autoLoadLastSearch", m_autoLoadCheck->isChecked());
    settings.setValue("app/themeMode", m_themeCombo->currentData().toInt());
    settings.setValue("app/highlightColor", m_selectedColor.name());
    settings.setValue("index/batchSize", m_batchSizeSpin->value());
    settings.setValue("index/enableSpelling", m_spellingCheck->isChecked());

    accept();
}

void PreferencesDialog::onPickColor()
{
    QColor color = QColorDialog::getColor(m_selectedColor, this, tr("选择高亮颜色"));
    if (color.isValid()) {
        m_selectedColor = color;
        m_colorBtn->setStyleSheet(
            QString("background-color: %1; border: 1px solid #888; border-radius: 3px;")
                .arg(color.name()));
    }
}

// ---- Getters ----
QString PreferencesDialog::defaultScope() const { return m_scopeCombo->currentData().toString(); }
int PreferencesDialog::pageSize() const { return m_pageSizeSpin->value(); }
bool PreferencesDialog::autoLoadLastSearch() const { return m_autoLoadCheck->isChecked(); }
int PreferencesDialog::themeMode() const { return m_themeCombo->currentData().toInt(); }
QColor PreferencesDialog::highlightColor() const { return m_selectedColor; }
int PreferencesDialog::batchSize() const { return m_batchSizeSpin->value(); }
bool PreferencesDialog::enableSpelling() const { return m_spellingCheck->isChecked(); }
