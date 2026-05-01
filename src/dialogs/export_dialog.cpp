/*
 * export_dialog.cpp - 导出对话框实现

实现导出对话框的 UI 布局和交互逻辑。
 */

#include <QGroupBox>
#include "export_dialog.h"
#include <QFileDialog>
#include <QMessageBox>

ExportDialog::ExportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("导出结果");
    setMinimumSize(500, 300);
    setupUI();
}

void ExportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

    // Format selection
    auto* formatLayout = new QFormLayout();
    m_formatCombo = new QComboBox();
    m_formatCombo->addItems({"CSV 文件 (*.csv)", "JSON 文件 (*.json)", "纯文本 (*.txt)"});
    formatLayout->addRow("导出格式:", m_formatCombo);
    mainLayout->addLayout(formatLayout);

    // Output path
    auto* pathLayout = new QHBoxLayout();
    m_outputPath = new QLineEdit();
    m_outputPath->setPlaceholderText("选择输出路径...");
    m_outputPath->setReadOnly(true);
    m_browseBtn = new QPushButton("浏览...");
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::onBrowseOutput);
    pathLayout->addWidget(m_outputPath, 1);
    pathLayout->addWidget(m_browseBtn);
    mainLayout->addLayout(pathLayout);

    // Export range
    auto* rangeGroupBox = new QGroupBox("导出范围");
    auto* rangeLayout = new QVBoxLayout(rangeGroupBox);
    m_exportRangeGroup = new QButtonGroup(this);

    m_allResultsRadio = new QRadioButton("全部结果");
    m_selectedResultsRadio = new QRadioButton("选定结果");
    m_currentPageRadio = new QRadioButton("当前页");
    m_allResultsRadio->setChecked(true);

    m_exportRangeGroup->addButton(m_allResultsRadio, 0);
    m_exportRangeGroup->addButton(m_selectedResultsRadio, 1);
    m_exportRangeGroup->addButton(m_currentPageRadio, 2);

    rangeLayout->addWidget(m_allResultsRadio);
    rangeLayout->addWidget(m_selectedResultsRadio);
    rangeLayout->addWidget(m_currentPageRadio);
    mainLayout->addWidget(rangeGroupBox);

    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    mainLayout->addWidget(m_progressBar);

    mainLayout->addStretch();

    // Buttons
    auto* btnLayout = new QHBoxLayout();
    m_startBtn = new QPushButton("开始导出");
    m_startBtn->setDefault(true);
    m_cancelBtn = new QPushButton("取消");
    connect(m_startBtn, &QPushButton::clicked, this, &ExportDialog::onStartExport);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addStretch();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);
    mainLayout->addLayout(btnLayout);
}

void ExportDialog::onBrowseOutput()
{
    QString filter;
    switch (m_formatCombo->currentIndex()) {
        case 0: filter = "CSV 文件 (*.csv)"; break;
        case 1: filter = "JSON 文件 (*.json)"; break;
        case 2: filter = "文本文件 (*.txt)"; break;
        default: filter = "所有文件 (*)"; break;
    }

    QString filePath = QFileDialog::getSaveFileName(this, "选择导出路径", QString(), filter);
    if (!filePath.isEmpty()) {
        m_outputPath->setText(filePath);
    }
}

void ExportDialog::onStartExport()
{
    if (m_outputPath->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "请选择输出路径");
        return;
    }

    QString format;
    switch (m_formatCombo->currentIndex()) {
        case 0: format = "csv"; break;
        case 1: format = "json"; break;
        case 2: format = "text"; break;
        default: format = "csv"; break;
    }

    bool allResults = (m_exportRangeGroup->checkedId() == 0);
    emit exportRequested(format, m_outputPath->text(), allResults);

    m_progressBar->setVisible(true);
    m_startBtn->setEnabled(false);
}
