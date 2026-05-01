#include "gui/file_panel.h"
#include "utils/file_utils.h"
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QDebug>

FilePanel::FilePanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void FilePanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Header with count
    m_countLabel = new QLabel(tr("文件"), this);
    m_countLabel->setStyleSheet(
        "font-weight: bold; color: #d4d4d4; padding: 6px 8px;");
    layout->addWidget(m_countLabel);

    // Type filter (multi-select)
    auto* typeLabel = new QLabel(tr("类型:"), this);
    typeLabel->setStyleSheet("color: #d4d4d4; padding: 4px 8px 0 8px;");
    layout->addWidget(typeLabel);

    m_typeList = new QListWidget(this);
    m_typeList->setMaximumHeight(120);
    m_typeList->setSelectionMode(QAbstractItemView::MultiSelection);
    m_typeList->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; color: #d4d4d4; "
        "border: none; font-size: 11px; }"
        "QListWidget::item { padding: 2px 8px; }"
        "QListWidget::item:selected { background-color: #094771; }");

    auto addTypeItem = [this](const QString& label, const QString& data) {
        auto* item = new QListWidgetItem(label, m_typeList);
        item->setData(Qt::UserRole, data);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked); // all selected by default
    };
    addTypeItem(tr("全部"), "");
    addTypeItem(tr("文本"), "txt");
    addTypeItem(tr("PDF"), "pdf");
    addTypeItem(tr("DOCX"), "docx");
    addTypeItem(tr("图片"), "img");
    layout->addWidget(m_typeList);

    // Sort controls
    auto* sortRow = new QHBoxLayout();
    auto* sortLabel = new QLabel(tr("排序:"), this);
    sortLabel->setStyleSheet("color: #d4d4d4; padding-left: 8px;");
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("文件名"), 0);
    m_sortCombo->addItem(tr("修改时间"), 1);
    m_sortCombo->addItem(tr("相关性"), 2);
    m_sortCombo->setCurrentIndex(2);  // Default: relevance
    m_sortCombo->setStyleSheet(
        "QComboBox { background-color: #3c3c3c; color: #d4d4d4; "
        "border: 1px solid #555; border-radius: 3px; padding: 2px 4px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background-color: #3c3c3c; "
        "color: #d4d4d4; selection-background-color: #094771; }");

    m_sortOrderBtn = new QPushButton(tr("↓"), this);
    m_sortOrderBtn->setFixedSize(24, 24);
    m_sortOrderBtn->setStyleSheet(
        "QPushButton { border: 1px solid #555; color: #d4d4d4; "
        "border-radius: 3px; background-color: #3c3c3c; }"
        "QPushButton:hover { background-color: #4c4c4c; }");

    sortRow->addWidget(sortLabel);
    sortRow->addWidget(m_sortCombo, 1);
    sortRow->addWidget(m_sortOrderBtn);
    layout->addLayout(sortRow);

    // File list (flat list, not tree)
    m_list = new QListWidget(this);
    m_list->setStyleSheet(
        "QListWidget { background-color: #1e1e1e; color: #d4d4d4; "
        "border: none; font-size: 12px; }"
        "QListWidget::item { padding: 6px 8px; border-bottom: 1px solid #2d2d2d; }"
        "QListWidget::item:selected { background-color: #094771; }"
        "QListWidget::item:hover { background-color: #2a2d2e; }");
    m_list->setWordWrap(false);
    m_list->setSpacing(0);

    layout->addWidget(m_list, 1);

    // Connections
    auto selectItem = [this](QListWidgetItem* item) {
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_documents.size()) {
            emit fileSelected(m_documents[idx]);
        }
    };

    connect(m_list, &QListWidget::itemClicked, this, selectItem);
    connect(m_list, &QListWidget::itemActivated, this, selectItem);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QListWidgetItem* item = m_list->itemAt(pos);
        if (!item) return;
        int idx = item->data(Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_documents.size()) return;

        QMenu menu(this);
        QAction* excludeAction = menu.addAction(tr("排除此文件"));
        if (menu.exec(m_list->viewport()->mapToGlobal(pos)) == excludeAction) {
            emit excludePath(m_documents[idx].filePath);
        }
    });

    connect(m_typeList, &QListWidget::itemChanged, this, &FilePanel::onTypeFilterChanged);
    connect(m_typeList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        // Toggle check state on click
        item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        onTypeFilterChanged();
    });
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onSortChanged);
    connect(m_sortOrderBtn, &QPushButton::clicked, this, [this]() {
        m_sortReverse = !m_sortReverse;
        m_sortOrderBtn->setText(m_sortReverse ? tr("↑") : tr("↓"));
        applySort();
    });
}

void FilePanel::setFiles(const QVector<Document>& docs)
{
    m_documents = docs;
    applySort();
}

void FilePanel::applySort()
{
    m_list->clear();
    int sortMode = m_sortCombo->currentData().toInt();
    // Collect selected types from multi-select list
    QStringList selectedTypes;
    for (int i = 0; i < m_typeList->count(); ++i) {
        auto* item = m_typeList->item(i);
        if (item->checkState() == Qt::Checked) {
            selectedTypes.append(item->data(Qt::UserRole).toString());
        }
    }
    bool allTypes = selectedTypes.contains("") || selectedTypes.size() == m_typeList->count();

    // Filter + collect indices
    QVector<int> indices;
    indices.reserve(m_documents.size());
    for (int i = 0; i < m_documents.size(); ++i) {
        const auto& doc = m_documents[i];

        // When search active, only show matching files
        if (m_hasActiveSearch && !m_matchIds.contains(doc.docId)) {
            continue;
        }

        // Apply type filter (multi-select)
        if (!allTypes) {
            QString ext = doc.fileExt.toLower();
            bool typeMatch = false;
            for (const auto& t : selectedTypes) {
                if (t == "img") {
                    QStringList imgExts = {"png","jpg","jpeg","gif","bmp","tiff","webp"};
                    if (imgExts.contains(ext)) { typeMatch = true; break; }
                } else if (ext == t) {
                    typeMatch = true;
                    break;
                }
            }
            if (!typeMatch) continue;
        }

        indices.append(i);
    }

    // Sort indices
    std::sort(indices.begin(), indices.end(), [this, sortMode](int a, int b) {
        int cmp = sortValue(m_documents[a], sortMode) - sortValue(m_documents[b], sortMode);
        if (cmp == 0) return a < b;
        return m_sortReverse ? cmp > 0 : cmp < 0;
    });

    // Populate list (lazy: max 2000 items, rest shown as count)
    int maxItems = 2000;
    int shown = 0;
    for (int idx : indices) {
        if (shown >= maxItems) break;
        const auto& doc = m_documents[idx];
        QFileInfo fi(doc.filePath);
        auto* item = new QListWidgetItem();
        item->setText(fi.fileName());
        item->setToolTip(doc.filePath);
        item->setData(Qt::UserRole, idx);
        m_list->addItem(item);
        shown++;
    }

    // Show overflow indicator
    if (indices.size() > maxItems) {
        auto* item = new QListWidgetItem();
        item->setText(tr("... 还有 %1 个文件").arg(indices.size() - maxItems));
        item->setForeground(QBrush(QColor("#888")));
        m_list->addItem(item);
    }

    if (m_hasActiveSearch) {
        m_countLabel->setText(QString(tr("匹配 (%1)")).arg(indices.size()));
    } else {
        m_countLabel->setText(QString(tr("文件 (%1)")).arg(indices.size()));
    }
}

void FilePanel::onTypeFilterChanged()
{
    applySort();
}

int FilePanel::sortValue(const Document& doc, int mode) const
{
    switch (mode) {
        case 1: // time
            return static_cast<int>(doc.modifiedTime);
        case 2: // relevance
            return doc.percent;
        default: // filename
        {
            QFileInfo fi(doc.filePath);
            QString name = fi.fileName().toLower();
            // Simple hash-like comparison for sorting
            int val = 0;
            for (int i = 0; i < name.size() && i < 20; ++i) {
                val = val * 31 + name[i].unicode();
            }
            return val;
        }
    }
}

void FilePanel::setMatchIds(const QSet<int64_t>& matchDocIds)
{
    m_hasActiveSearch = true;
    m_matchIds = matchDocIds;
    applySort();
    m_countLabel->setText(QString(tr("文件 (%1 / %2 匹配)"))
        .arg(m_matchIds.size()).arg(m_documents.size()));
}

void FilePanel::clear()
{
    m_documents.clear();
    m_matchIds.clear();
    m_hasActiveSearch = false;
    m_list->clear();
    m_countLabel->setText(tr("文件"));
}

void FilePanel::onSortChanged(int /*index*/)
{
    applySort();
}
