/*
 * file_panel.cpp - 文件列表面板 (Windows 资源管理器风格)
 */

#include "gui/file_panel.h"
#include "utils/file_utils.h"
#include <QFileInfo>
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QIcon>
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

    // Header row: type filter + count
    auto* headerRow = new QHBoxLayout();

    m_countLabel = new QLabel(tr("文件"), this);
    m_countLabel->setStyleSheet("font-weight: bold; padding: 6px 8px;");
    headerRow->addWidget(m_countLabel);
    headerRow->addStretch();

    auto* typeLabel = new QLabel(tr("类型:"), this);
    typeLabel->setStyleSheet("padding: 4px 0; font-size: 11px;");
    headerRow->addWidget(typeLabel);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem(tr("全部"), "");
    m_typeCombo->addItem(tr("文本"), "txt");
    m_typeCombo->addItem(tr("PDF"), "pdf");
    m_typeCombo->addItem(tr("DOCX"), "docx");
    m_typeCombo->addItem(tr("图片"), "img");
    m_typeCombo->setFixedWidth(80);
    m_typeCombo->setStyleSheet(
        "QComboBox { border: 1px solid #c0c0c0; border-radius: 3px; "
        "padding: 2px 4px; font-size: 11px; }"
        "QComboBox::drop-down { border: none; width: 16px; }");
    headerRow->addWidget(m_typeCombo);

    layout->addLayout(headerRow);

    // File tree view (Windows Explorer style)
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(4);
    m_tree->setHeaderLabels({
        tr("名称"), tr("修改日期"), tr("大小"), tr("类型")
    });
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSortingEnabled(false);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSortIndicatorShown(true);
    m_tree->setColumnWidth(0, 180);

    QString baseStyle =
        "QTreeWidget { border: none; font-size: 12px; }"
        "QTreeWidget::item { padding: 3px 2px; }"
        "QTreeWidget::item:selected { background-color: #0060C0; color: white; }"
        "QTreeWidget::item:hover { background-color: #E8F0FE; }";
    m_tree->setStyleSheet(baseStyle);

    layout->addWidget(m_tree, 1);

    // Connections
    connect(m_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_documents.size())
            emit fileSelected(m_documents[idx]);
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int) {
        if (!item) return;
        int idx = item->data(0, Qt::UserRole).toInt();
        if (idx >= 0 && idx < m_documents.size())
            emit fileSelected(m_documents[idx]);
    });

    connect(m_tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_tree->itemAt(pos);
        if (!item) return;
        int idx = item->data(0, Qt::UserRole).toInt();
        if (idx < 0 || idx >= m_documents.size()) return;
        QMenu menu(this);
        QAction* excludeAction = menu.addAction(tr("排除此文件"));
        if (menu.exec(m_tree->viewport()->mapToGlobal(pos)) == excludeAction)
            emit excludePath(m_documents[idx].filePath);
    });

    connect(m_tree->header(), &QHeaderView::sectionClicked, this, &FilePanel::onHeaderClicked);
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FilePanel::onTypeFilterChanged);
}

void FilePanel::setFiles(const QVector<Document>& docs)
{
    m_documents = docs;
    applySort();
}

void FilePanel::applySort()
{
    m_tree->clear();

    QString selectedType = m_typeCombo->currentData().toString();
    bool allTypes = selectedType.isEmpty();

    // Collect matching indices
    QVector<int> indices;
    indices.reserve(m_documents.size());
    for (int i = 0; i < m_documents.size(); ++i) {
        const auto& doc = m_documents[i];
        if (m_hasActiveSearch && !m_matchIds.contains(doc.docId))
            continue;
        if (!allTypes) {
            QString ext = doc.fileExt.toLower();
            if (selectedType == "img") {
                QStringList imgExts = {"png","jpg","jpeg","gif","bmp","tiff","webp"};
                if (!imgExts.contains(ext)) continue;
            } else if (ext != selectedType) {
                continue;
            }
        }
        indices.append(i);
    }

    // Sort
    std::sort(indices.begin(), indices.end(), [this](int a, int b) {
        const auto& da = m_documents[a];
        const auto& db = m_documents[b];
        int cmp = 0;
        switch (m_sortColumn) {
            case 0: cmp = QString::compare(da.fileName.toLower(), db.fileName.toLower()); break;
            case 1: cmp = (da.modifiedTime > db.modifiedTime) - (da.modifiedTime < db.modifiedTime); break;
            case 2: cmp = (da.fileSize > db.fileSize) - (da.fileSize < db.fileSize); break;
            case 3: cmp = QString::compare(da.fileExt.toLower(), db.fileExt.toLower()); break;
        }
        return m_sortOrder == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
    });

    // Populate tree
    int maxItems = 2000;
    int shown = 0;
    for (int idx : indices) {
        if (shown >= maxItems) break;
        createFileRow(m_documents[idx], idx);
        shown++;
    }

    if (indices.size() > maxItems) {
        auto* footer = new QTreeWidgetItem(m_tree);
        footer->setText(0, tr("... 还有 %1 个文件").arg(indices.size() - maxItems));
        footer->setForeground(0, QBrush(QColor("#888")));
    }

    QString label = m_hasActiveSearch
        ? tr("匹配 (%1)").arg(indices.size())
        : tr("文件 (%1)").arg(indices.size());
    m_countLabel->setText(label);

    m_tree->header()->setSortIndicator(m_sortColumn, m_sortOrder);
}

QTreeWidgetItem* FilePanel::createFileRow(const Document& doc, int idx)
{
    auto* item = new QTreeWidgetItem(m_tree);

    QFileInfo fi(doc.filePath);

    // Name column with icon
    item->setText(0, fi.fileName());
    item->setToolTip(0, doc.filePath);
    item->setData(0, Qt::UserRole, idx);
    QIcon icon = FileUtils::getFileIcon(doc.filePath);
    if (!icon.isNull())
        item->setIcon(0, icon);

    // Date modified
    item->setText(1, QDateTime::fromSecsSinceEpoch(doc.modifiedTime)
                     .toString("yyyy-MM-dd HH:mm"));

    // Size
    item->setText(2, FileUtils::formatFileSize(doc.fileSize));
    item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);

    // Type
    QString ext = doc.fileExt.toUpper();
    if (ext.isEmpty()) ext = tr("文件");
    item->setText(3, ext);

    return item;
}

void FilePanel::onTypeFilterChanged(int /*index*/)
{
    applySort();
}

void FilePanel::onHeaderClicked(int logicalIndex)
{
    if (logicalIndex == m_sortColumn) {
        m_sortOrder = (m_sortOrder == Qt::AscendingOrder)
            ? Qt::DescendingOrder : Qt::AscendingOrder;
    } else {
        m_sortColumn = logicalIndex;
        m_sortOrder = Qt::AscendingOrder;
    }
    applySort();
}

int FilePanel::currentSortColumn() const
{
    return m_sortColumn;
}

void FilePanel::setMatchIds(const QSet<int64_t>& matchDocIds)
{
    m_hasActiveSearch = true;
    m_matchIds = matchDocIds;
    applySort();
    m_countLabel->setText(tr("文件 (%1 / %2 匹配)")
        .arg(m_matchIds.size()).arg(m_documents.size()));
}

void FilePanel::clear()
{
    m_documents.clear();
    m_matchIds.clear();
    m_hasActiveSearch = false;
    m_tree->clear();
    m_countLabel->setText(tr("文件"));
}
