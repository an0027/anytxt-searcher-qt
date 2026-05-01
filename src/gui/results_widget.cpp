/*
 * results_widget.cpp - 搜索结果列表实现

实现结果列表的表格渲染、排序逻辑、分页控制、
右键菜单和文件交互。
 */

#include "gui/results_widget.h"
#include "utils/file_utils.h"
#include <QMenu>
#include <QAction>
#include <QDateTime>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QDebug>


ResultsWidget::ResultsWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void ResultsWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Toolbar row
    auto* toolbar = new QHBoxLayout();

    m_resultsCountLabel = new QLabel(tr("找到 0 个结果"), this);
    m_resultsCountLabel->setStyleSheet("font-weight: bold; color: #555; padding: 2px 4px;");

    toolbar->addWidget(m_resultsCountLabel);
    toolbar->addStretch();

    toolbar->addWidget(new QLabel(tr("排序:"), this));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItem(tr("相关性"), "relevance");
    m_sortCombo->addItem(tr("日期"), "date");
    m_sortCombo->addItem(tr("大小"), "size");
    m_sortCombo->addItem(tr("名称"), "name");
    toolbar->addWidget(m_sortCombo);

    m_sortOrderBtn = new QPushButton(tr("↓"), this);
    m_sortOrderBtn->setFixedSize(28, 28);
    m_sortOrderBtn->setToolTip(tr("切换排序顺序"));
    m_sortOrderBtn->setStyleSheet(
        "QPushButton { border: 1px solid #ccc; border-radius: 4px; }"
        "QPushButton:hover { background-color: #e0e0e0; }");
    toolbar->addWidget(m_sortOrderBtn);

    layout->addLayout(toolbar);

    // Tree widget
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({
        tr("名称"), tr("路径"), tr("大小"),
        tr("修改时间"), tr("类型"), tr("相关性")
    });
    m_tree->setAlternatingRowColors(true);
    m_tree->setRootIsDecorated(false);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_tree->setSortingEnabled(false);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_tree->setColumnWidth(0, 200);

    layout->addWidget(m_tree, 1);

    // Pagination row
    auto* paginationRow = new QHBoxLayout();
    paginationRow->addStretch();

    m_prevPageBtn = new QPushButton(tr("← 上一页"), this);
    m_prevPageBtn->setEnabled(false);
    m_prevPageBtn->setStyleSheet(
        "QPushButton { border: 1px solid #ccc; border-radius: 4px; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
        "QPushButton:disabled { color: #999; }");

    m_pageLabel = new QLabel(tr("第 1 页"), this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_pageLabel->setMinimumWidth(60);

    m_nextPageBtn = new QPushButton(tr("下一页 →"), this);
    m_nextPageBtn->setEnabled(false);
    m_nextPageBtn->setStyleSheet(
        "QPushButton { border: 1px solid #ccc; border-radius: 4px; padding: 4px 12px; }"
        "QPushButton:hover { background-color: #e0e0e0; }"
        "QPushButton:disabled { color: #999; }");

    paginationRow->addWidget(m_prevPageBtn);
    paginationRow->addWidget(m_pageLabel);
    paginationRow->addWidget(m_nextPageBtn);
    paginationRow->addStretch();

    layout->addLayout(paginationRow);

    // Connections
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &ResultsWidget::onItemSelectionChanged);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, &ResultsWidget::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ResultsWidget::onCustomContextMenu);
    connect(m_prevPageBtn, &QPushButton::clicked,
            this, &ResultsWidget::onPreviousPage);
    connect(m_nextPageBtn, &QPushButton::clicked,
            this, &ResultsWidget::onNextPage);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ResultsWidget::onSortComboChanged);
    connect(m_sortOrderBtn, &QPushButton::clicked, this, [this]() {
        m_sortReverse = !m_sortReverse;
        m_sortOrderBtn->setText(m_sortReverse ? tr("↑") : tr("↓"));
        emit sortChanged(m_sortCombo->currentData().toString(), m_sortReverse);
    });
}

void ResultsWidget::setResults(const QVector<Document>& documents, int totalResults)
{
    m_documents = documents;
    m_totalResults = totalResults;

    while (m_tree->topLevelItemCount() > 0) {
        delete m_tree->takeTopLevelItem(0);
    }

    for (int i = 0; i < documents.size(); ++i) {
        createItem(documents[i], i);
    }

    m_resultsCountLabel->setText(
        tr("找到 %1 个结果")
            .arg(m_totalResults)
    );

    updatePagination();
}

QTreeWidgetItem* ResultsWidget::createItem(const Document& doc, int row)
{
    auto* item = new QTreeWidgetItem(m_tree);
    item->setText(0, doc.fileName);
    item->setText(1, doc.filePath);
    item->setText(2, FileUtils::formatFileSize(doc.fileSize));
    item->setText(3, QDateTime::fromSecsSinceEpoch(doc.modifiedTime)
                      .toString("yyyy-MM-dd HH:mm"));
    item->setText(4, doc.fileExt.toUpper());
    item->setText(5, QString("%1%").arg(doc.percent));

    // Store doc ID in user data
    item->setData(0, Qt::UserRole, static_cast<qlonglong>(doc.docId));
    item->setData(0, Qt::UserRole + 1, doc.filePath);

    // Set icon if we can
    QIcon icon = FileUtils::getFileIcon(doc.filePath);
    if (!icon.isNull()) {
        item->setIcon(0, icon);
    }

    // Color coding for relevance
    if (doc.percent >= 80) {
        item->setForeground(5, QBrush(QColor("#2E7D32")));
    } else if (doc.percent >= 50) {
        item->setForeground(5, QBrush(QColor("#F57F17")));
    } else {
        item->setForeground(5, QBrush(QColor("#999")));
    }

    return item;
}

void ResultsWidget::clear()
{
    m_tree->clear();
    m_documents.clear();
    m_totalResults = 0;
    m_currentPage = 1;
    m_resultsCountLabel->setText(tr("找到 0 个结果"));
    updatePagination();
}

Document ResultsWidget::selectedDocument() const
{
    auto items = m_tree->selectedItems();
    if (items.isEmpty()) return {};

    int row = m_tree->indexOfTopLevelItem(items[0]);
    if (row >= 0 && row < m_documents.size()) {
        return m_documents[row];
    }
    return {};
}

void ResultsWidget::onItemSelectionChanged()
{
    Document doc = selectedDocument();
    if (doc.docId >= 0) {
        emit resultSelected(doc);
    }
}

void ResultsWidget::onItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);
    if (!item) return;
    int row = m_tree->indexOfTopLevelItem(item);
    if (row >= 0 && row < m_documents.size()) {
        emit openFile(m_documents[row]);
    }
}

void ResultsWidget::onCustomContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;

    int row = m_tree->indexOfTopLevelItem(item);
    if (row < 0 || row >= m_documents.size()) return;

    const Document& doc = m_documents[row];

    QMenu menu(this);
    QAction* openAction = menu.addAction(tr("打开文件"));
    QAction* openFolderAction = menu.addAction(tr("打开所在文件夹"));
    menu.addSeparator();
    QAction* copyPathAction = menu.addAction(tr("复制路径"));
    QAction* copyNameAction = menu.addAction(tr("复制文件名"));
    menu.addSeparator();
    QAction* excludeAction = menu.addAction(tr("排除此文件"));

    QAction* selected = menu.exec(m_tree->viewport()->mapToGlobal(pos));

    if (selected == openAction) {
        emit openFile(doc);
    } else if (selected == openFolderAction) {
        QFileInfo fi(doc.filePath);
        QDesktopServices::openUrl(
            QUrl::fromLocalFile(fi.absolutePath()));
    } else if (selected == copyPathAction) {
        QApplication::clipboard()->setText(doc.filePath);
        emit copyPath(doc.filePath);
    } else if (selected == copyNameAction) {
        QApplication::clipboard()->setText(doc.fileName);
    } else if (selected == excludeAction) {
        // TODO: implement exclude logic
        qDebug() << "Excluding:" << doc.filePath;
    }
}

void ResultsWidget::onPreviousPage()
{
    if (m_currentPage > 1) {
        m_currentPage--;
        emit pageChanged(m_currentPage);
    }
}

void ResultsWidget::onNextPage()
{
    int maxPage = qMax(1, (m_totalResults + m_pageSize - 1) / m_pageSize);
    if (m_currentPage < maxPage) {
        m_currentPage++;
        emit pageChanged(m_currentPage);
    }
}

void ResultsWidget::updatePagination()
{
    int maxPage = qMax(1, (m_totalResults + m_pageSize - 1) / m_pageSize);
    m_prevPageBtn->setEnabled(m_currentPage > 1);
    m_nextPageBtn->setEnabled(m_currentPage < maxPage);
    m_pageLabel->setText(tr("第 %1/%2 页").arg(m_currentPage).arg(maxPage));
}

void ResultsWidget::onSortComboChanged(int index)
{
    Q_UNUSED(index);
    emit sortChanged(m_sortCombo->currentData().toString(), m_sortReverse);
}
