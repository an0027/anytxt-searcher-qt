/*
 * file_panel.cpp - 文件浏览面板（QTreeView + 虚拟滚动）
 */

#include "file_panel.h"
#include "lazy_file_model.h"
#include <QVBoxLayout>

FilePanel::FilePanel(QWidget* parent) : QWidget(parent)
{
    setupUI();
}

void FilePanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_countLabel = new QLabel(this);
    m_countLabel->setStyleSheet("padding: 4px 8px; font-size: 12px; color: #888;");
    layout->addWidget(m_countLabel);

    m_model = new LazyFileModel(this);

    m_view = new QTreeView(this);
    m_view->setModel(m_model);
    m_view->setRootIsDecorated(false);
    m_view->setAlternatingRowColors(true);
    m_view->setSortingEnabled(true);
    m_view->sortByColumn(0, Qt::AscendingOrder);
    m_view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_view->setUniformRowHeights(true);  // optimization for large lists

    auto* header = m_view->header();
    header->setStretchLastSection(false);
    header->setSectionResizeMode(0, QHeaderView::Stretch);
    header->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSortIndicatorShown(true);

    connect(m_view, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) return;
        QVariant v = m_model->data(index.siblingAtColumn(0), Qt::UserRole);
        if (v.isValid()) {
            int64_t docId = v.toLongLong();
            int row = index.row();
            if (row >= 0 && row < m_model->documentCount())
                emit fileSelected(m_model->documentAt(row));
        }
    });

    layout->addWidget(m_view, 1);
}

void FilePanel::setFiles(const QVector<Document>& docs)
{
    m_model->setDocuments(docs);
    m_countLabel->setText(tr("共 %1 个文件").arg(docs.size()));
}

void FilePanel::clear()
{
    m_model->clear();
    m_matchIds.clear();
    m_countLabel->clear();
}
