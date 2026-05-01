#include "gui/file_panel.h"
#include "utils/file_utils.h"
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QDebug>

FilePanel::FilePanel(QWidget* parent) : QWidget(parent) { setupUI(); }

void FilePanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Header: count + sort controls
    auto* headerRow = new QHBoxLayout();
    m_countLabel = new QLabel(tr("文件"), this);
    m_countLabel->setStyleSheet("font-weight: bold; padding: 6px 8px;");
    headerRow->addWidget(m_countLabel);
    headerRow->addStretch();

    auto* sortLabel = new QLabel(tr("排序:"), this);
    sortLabel->setStyleSheet("font-size: 11px;");
    headerRow->addWidget(sortLabel);

    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("文件名"), 0);
    m_sortCombo->addItem(tr("相关性"), 1);
    m_sortCombo->addItem(tr("修改日期"), 2);
    m_sortCombo->addItem(tr("大小"), 3);
    m_sortCombo->setCurrentIndex(1);
    headerRow->addWidget(m_sortCombo);

    m_sortOrderBtn = new QPushButton(tr("v"), this);
    m_sortOrderBtn->setFixedSize(24, 24);
    headerRow->addWidget(m_sortOrderBtn);
    layout->addLayout(headerRow);

    // File list (single column)
    m_list = new QListWidget(this);
    m_list->setStyleSheet(
        "QListWidget { border: 1px solid #b0b0b0; border-top: 2px solid #c0c0c0; "
        "border-left: 2px solid #c0c0c0; font-size: 12px; }"
        "QListWidget::item { padding: 4px 8px; }");
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    layout->addWidget(m_list, 1);

    // Connections
    auto selectItem = [this](QListWidgetItem* item) {
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_documents.size())
            emit fileSelected(m_documents[idx]);
    };
    connect(m_list, &QListWidget::itemClicked, this, selectItem);
    connect(m_list, &QListWidget::itemActivated, this, selectItem);

    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_list->itemAt(pos);
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_documents.size()) return;
        QMenu menu(this);
        if (menu.addAction(tr("排除此文件")) == menu.exec(m_list->viewport()->mapToGlobal(pos)))
            emit excludePath(m_documents[idx].filePath);
    });

    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onSortChanged);
    connect(m_sortOrderBtn, &QPushButton::clicked, this, [this]() {
        m_sortReverse = !m_sortReverse;
        m_sortOrderBtn->setText(m_sortReverse ? tr("^") : tr("v"));
        applySort();
    });
}

void FilePanel::setFiles(const QVector<Document>& docs) { m_documents = docs; applySort(); }

void FilePanel::applySort()
{
    m_list->clear();

    // Collect indices
    QVector<int> indices;
    indices.reserve(m_documents.size());
    for (int i = 0; i < m_documents.size(); ++i) {
        if (m_hasActiveSearch && !m_matchIds.contains(m_documents[i].docId))
            continue;
        indices.append(i);
    }

    // Sort
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        const auto& da = m_documents[a];
        const auto& db = m_documents[b];
        int cmp = 0;
        switch (m_sortMode) {
            case 0: cmp = QString::compare(da.fileName.toLower(), db.fileName.toLower()); break;
            case 1: cmp = da.percent - db.percent; break;
            case 2: cmp = (da.modifiedTime > db.modifiedTime) - (da.modifiedTime < db.modifiedTime); break;
            case 3: cmp = (da.fileSize > db.fileSize) - (da.fileSize < db.fileSize); break;
        }
        if (cmp == 0) cmp = a - b;
        return m_sortReverse ? cmp > 0 : cmp < 0;
    });

    // Populate
    int maxItems = 2000, shown = 0;
    for (int idx : indices) {
        if (shown >= maxItems) break;
        const auto& doc = m_documents[idx];
        QFileInfo fi(doc.filePath);
        auto* item = new QListWidgetItem();
        item->setText(fi.fileName());
        item->setToolTip(doc.filePath);
        item->setData(Qt::UserRole, idx);

        // Color by relevance
        if (doc.percent >= 80)
            item->setForeground(QBrush(QColor("#1B5E20")));
        else if (doc.percent >= 50)
            item->setForeground(QBrush(QColor("#E65100")));
        else
            item->setForeground(QBrush(QColor("#888")));

        m_list->addItem(item);
        shown++;
    }
    if (indices.size() > maxItems) {
        auto* f = new QListWidgetItem();
        f->setText(tr("... %1 个文件").arg(indices.size() - maxItems));
        f->setForeground(QBrush(QColor("#888")));
        m_list->addItem(f);
    }
    m_countLabel->setText(m_hasActiveSearch
        ? tr("匹配 (%1)").arg(indices.size())
        : tr("文件 (%1)").arg(indices.size()));
}

void FilePanel::setMatchIds(const QSet<int64_t>& matchDocIds)
{
    m_hasActiveSearch = true;
    m_matchIds = matchDocIds;
    applySort();
    m_countLabel->setText(tr("文件 (%1 / %2 匹配)").arg(m_matchIds.size()).arg(m_documents.size()));
}

void FilePanel::clear()
{
    m_documents.clear(); m_matchIds.clear(); m_hasActiveSearch = false;
    m_list->clear();
    m_countLabel->setText(tr("文件"));
}

void FilePanel::onSortChanged(int /*index*/)
{
    m_sortMode = m_sortCombo->currentData().toInt();
    applySort();
}
