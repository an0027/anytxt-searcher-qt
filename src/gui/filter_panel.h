/*
 * filter_panel.h - 筛选面板

功能说明：侧边栏筛选面板，按文件类型、
修改日期、文件大小等维度过滤搜索结果。
 */

#ifndef ANYTXT_FILTER_PANEL_H
#define ANYTXT_FILTER_PANEL_H

#include <QWidget>
#include <QCheckBox>
#include <QRadioButton>
#include <QPushButton>
#include <QButtonGroup>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QVariantMap>

class FilterPanel : public QWidget {
    Q_OBJECT
public:
    explicit FilterPanel(QWidget* parent = nullptr);
    ~FilterPanel() override = default;

    QVariantMap getFilters() const;
    void clearFilters();
    void setFilters(const QVariantMap& filters);

signals:
    void filtersChanged(const QVariantMap& filters);

private slots:
    void onFilterChanged();

private:
    void setupUI();

    // File type checkboxes
    QCheckBox* m_allTypesCheck;
    QCheckBox* m_pdfCheck;
    QCheckBox* m_docxCheck;
    QCheckBox* m_txtCheck;
    QCheckBox* m_imagesCheck;
    QCheckBox* m_codeCheck;
    QCheckBox* m_archivesCheck;

    // Date modified radio buttons
    QButtonGroup* m_dateGroup;
    QRadioButton* m_anyDateRadio;
    QRadioButton* m_todayRadio;
    QRadioButton* m_pastWeekRadio;
    QRadioButton* m_pastMonthRadio;
    QRadioButton* m_pastYearRadio;

    // File size radio buttons
    QButtonGroup* m_sizeGroup;
    QRadioButton* m_anySizeRadio;
    QRadioButton* m_smallRadio;
    QRadioButton* m_mediumRadio;
    QRadioButton* m_largeRadio;

    QPushButton* m_clearBtn;
};

#endif // ANYTXT_FILTER_PANEL_H
