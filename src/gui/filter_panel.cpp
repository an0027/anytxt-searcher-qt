/*
 * filter_panel.cpp - 筛选面板实现

实现筛选选项的 UI 布局和信号触发逻辑。
 */

#include "gui/filter_panel.h"
#include <QLabel>
#include <QDateTime>
#include <QScrollArea>
#include <QDebug>

FilterPanel::FilterPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void FilterPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Title
    auto* titleLabel = new QLabel(tr("<b>筛选条件</b>"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 14px; padding: 8px 0;");
    mainLayout->addWidget(titleLabel);

    // --- File Type group ---
    auto* typeGroup = new QGroupBox(tr("文件类型"), this);
    auto* typeLayout = new QVBoxLayout(typeGroup);
    typeLayout->setSpacing(4);

    m_allTypesCheck = new QCheckBox(tr("全部"), this);
    m_allTypesCheck->setChecked(true);
    m_pdfCheck = new QCheckBox("PDF", this);
    m_docxCheck = new QCheckBox("DOCX", this);
    m_txtCheck = new QCheckBox("TXT", this);
    m_imagesCheck = new QCheckBox(tr("图片"), this);
    m_codeCheck = new QCheckBox(tr("代码"), this);
    m_archivesCheck = new QCheckBox(tr("压缩包"), this);

    typeLayout->addWidget(m_allTypesCheck);
    typeLayout->addWidget(m_pdfCheck);
    typeLayout->addWidget(m_docxCheck);
    typeLayout->addWidget(m_txtCheck);
    typeLayout->addWidget(m_imagesCheck);
    typeLayout->addWidget(m_codeCheck);
    typeLayout->addWidget(m_archivesCheck);

    mainLayout->addWidget(typeGroup);

    // --- Date Modified group ---
    auto* dateGroupBox = new QGroupBox(tr("修改时间"), this);
    auto* dateLayout = new QVBoxLayout(dateGroupBox);
    dateLayout->setSpacing(4);

    m_dateGroup = new QButtonGroup(this);
    m_anyDateRadio = new QRadioButton(tr("任意时间"), this);
    m_anyDateRadio->setChecked(true);
    m_todayRadio = new QRadioButton(tr("今天"), this);
    m_pastWeekRadio = new QRadioButton(tr("过去一周"), this);
    m_pastMonthRadio = new QRadioButton(tr("过去一月"), this);
    m_pastYearRadio = new QRadioButton(tr("过去一年"), this);

    m_dateGroup->addButton(m_anyDateRadio, 0);
    m_dateGroup->addButton(m_todayRadio, 1);
    m_dateGroup->addButton(m_pastWeekRadio, 2);
    m_dateGroup->addButton(m_pastMonthRadio, 3);
    m_dateGroup->addButton(m_pastYearRadio, 4);

    dateLayout->addWidget(m_anyDateRadio);
    dateLayout->addWidget(m_todayRadio);
    dateLayout->addWidget(m_pastWeekRadio);
    dateLayout->addWidget(m_pastMonthRadio);
    dateLayout->addWidget(m_pastYearRadio);

    mainLayout->addWidget(dateGroupBox);

    // --- File Size group ---
    auto* sizeGroupBox = new QGroupBox(tr("文件大小"), this);
    auto* sizeLayout = new QVBoxLayout(sizeGroupBox);
    sizeLayout->setSpacing(4);

    m_sizeGroup = new QButtonGroup(this);
    m_anySizeRadio = new QRadioButton(tr("任意大小"), this);
    m_anySizeRadio->setChecked(true);
    m_smallRadio = new QRadioButton(tr("小于 1 MB"), this);
    m_mediumRadio = new QRadioButton(tr("1 - 100 MB"), this);
    m_largeRadio = new QRadioButton(tr("大于 100 MB"), this);

    m_sizeGroup->addButton(m_anySizeRadio, 0);
    m_sizeGroup->addButton(m_smallRadio, 1);
    m_sizeGroup->addButton(m_mediumRadio, 2);
    m_sizeGroup->addButton(m_largeRadio, 3);

    sizeLayout->addWidget(m_anySizeRadio);
    sizeLayout->addWidget(m_smallRadio);
    sizeLayout->addWidget(m_mediumRadio);
    sizeLayout->addWidget(m_largeRadio);

    mainLayout->addWidget(sizeGroupBox);

    // --- Clear button ---
    m_clearBtn = new QPushButton(tr("清除筛选"), this);
    m_clearBtn->setStyleSheet(
        "QPushButton { background-color: #f44336; color: white; border: none; "
        "border-radius: 4px; padding: 8px 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #d32f2f; }"
    );
    mainLayout->addWidget(m_clearBtn);

    mainLayout->addStretch();

    // Set a fixed width for the panel
    setMinimumWidth(180);
    setMaximumWidth(220);

    // Connect signals
    auto connectCheckbox = [this](QCheckBox* cb) {
        connect(cb, &QCheckBox::toggled, this, &FilterPanel::onFilterChanged);
    };
    connectCheckbox(m_allTypesCheck);
    connectCheckbox(m_pdfCheck);
    connectCheckbox(m_docxCheck);
    connectCheckbox(m_txtCheck);
    connectCheckbox(m_imagesCheck);
    connectCheckbox(m_codeCheck);
    connectCheckbox(m_archivesCheck);

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    connect(m_dateGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &FilterPanel::onFilterChanged);
    connect(m_sizeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &FilterPanel::onFilterChanged);
#else
    connect(m_dateGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &FilterPanel::onFilterChanged);
    connect(m_sizeGroup, QOverload<int>::of(&QButtonGroup::buttonClicked),
            this, &FilterPanel::onFilterChanged);
#endif
    connect(m_clearBtn, &QPushButton::clicked, this, &FilterPanel::clearFilters);

    // Connect "All" checkbox to toggle others
    connect(m_allTypesCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_pdfCheck->setEnabled(!checked);
        m_docxCheck->setEnabled(!checked);
        m_txtCheck->setEnabled(!checked);
        m_imagesCheck->setEnabled(!checked);
        m_codeCheck->setEnabled(!checked);
        m_archivesCheck->setEnabled(!checked);
    });
}

QVariantMap FilterPanel::getFilters() const
{
    QVariantMap filters;

    // File type filters
    if (!m_allTypesCheck->isChecked()) {
        QStringList selectedTypes;

        if (m_pdfCheck->isChecked()) selectedTypes << "pdf";
        if (m_docxCheck->isChecked()) selectedTypes << "docx" << "docm";
        if (m_txtCheck->isChecked()) selectedTypes << "txt" << "md" << "csv" << "log" << "xml" << "json";
        if (m_imagesCheck->isChecked()) selectedTypes << "png" << "jpg" << "jpeg" << "gif" << "bmp" << "webp";
        if (m_codeCheck->isChecked()) selectedTypes << "py" << "js" << "ts" << "c" << "cpp" << "h" << "hpp" << "java" << "go" << "rs" << "sh" << "rb" << "php";
        if (m_archivesCheck->isChecked()) selectedTypes << "zip" << "tar" << "gz" << "7z" << "rar";

        if (!selectedTypes.isEmpty()) {
            filters["ext"] = selectedTypes.join(",");
        }
    }

    // Date filters
    int dateId = m_dateGroup->checkedId();
    qint64 now = QDateTime::currentSecsSinceEpoch();
    switch (dateId) {
        case 1: // Today
            filters["date_after"] = QDateTime::currentDateTime()
                                        .date().startOfDay().toSecsSinceEpoch();
            break;
        case 2: // Past week
            filters["date_after"] = now - 7 * 24 * 3600;
            break;
        case 3: // Past month
            filters["date_after"] = now - 30 * 24 * 3600;
            break;
        case 4: // Past year
            filters["date_after"] = now - 365 * 24 * 3600;
            break;
    }

    // Size filters
    int sizeId = m_sizeGroup->checkedId();
    switch (sizeId) {
        case 1: // Small (< 1 MB)
            filters["size_max"] = static_cast<qint64>(1 * 1024 * 1024);
            break;
        case 2: // Medium (1-100 MB)
            filters["size_min"] = static_cast<qint64>(1 * 1024 * 1024);
            filters["size_max"] = static_cast<qint64>(100 * 1024 * 1024);
            break;
        case 3: // Large (> 100 MB)
            filters["size_min"] = static_cast<qint64>(100 * 1024 * 1024);
            break;
    }

    return filters;
}

void FilterPanel::clearFilters()
{
    m_allTypesCheck->setChecked(true);
    m_pdfCheck->setChecked(false);
    m_docxCheck->setChecked(false);
    m_txtCheck->setChecked(false);
    m_imagesCheck->setChecked(false);
    m_codeCheck->setChecked(false);
    m_archivesCheck->setChecked(false);

    m_anyDateRadio->setChecked(true);
    m_anySizeRadio->setChecked(true);

    emit filtersChanged(getFilters());
}

void FilterPanel::setFilters(const QVariantMap& filters)
{
    Q_UNUSED(filters);
    clearFilters();
}

void FilterPanel::onFilterChanged()
{
    emit filtersChanged(getFilters());
}
