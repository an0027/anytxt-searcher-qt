/*
 * export_dialog.h - 导出对话框

功能说明：导出搜索结果的对话框，支持 CSV、JSON、
纯文本格式，可选择导出范围（全部/选定/当前页）。
 */

#ifndef ANYTXT_EXPORT_DIALOG_H
#define ANYTXT_EXPORT_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>

class ExportDialog : public QDialog {
    Q_OBJECT
public:
    explicit ExportDialog(QWidget* parent = nullptr);
    ~ExportDialog() override = default;

signals:
    void exportRequested(const QString& format, const QString& outputPath, bool allResults);

private slots:
    void onBrowseOutput();
    void onStartExport();

private:
    void setupUI();

    QComboBox* m_formatCombo;
    QLineEdit* m_outputPath;
    QPushButton* m_browseBtn;

    QButtonGroup* m_exportRangeGroup;
    QRadioButton* m_allResultsRadio;
    QRadioButton* m_selectedResultsRadio;
    QRadioButton* m_currentPageRadio;

    QPushButton* m_startBtn;
    QPushButton* m_cancelBtn;
    QProgressBar* m_progressBar;
};

#endif // ANYTXT_EXPORT_DIALOG_H
