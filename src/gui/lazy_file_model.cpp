/*
 * lazy_file_model.cpp - 虚拟滚动文件列表模型实现
 */

#include "lazy_file_model.h"
#include <QFileInfo>
#include <algorithm>

LazyFileModel::LazyFileModel(QObject* parent)
    : QAbstractItemModel(parent)
{
}

LazyFileModel::~LazyFileModel() = default;

void LazyFileModel::setDocuments(const QVector<Document>& docs)
{
    beginResetModel();
    m_documents = docs;
    endResetModel();
}

void LazyFileModel::clear()
{
    beginResetModel();
    m_documents.clear();
    endResetModel();
}

QModelIndex LazyFileModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!parent.isValid() && row >= 0 && row < m_documents.size() && column >= 0 && column < ColCount)
        return createIndex(row, column, quintptr(0));
    return QModelIndex();
}

QModelIndex LazyFileModel::parent(const QModelIndex&) const
{
    return QModelIndex(); // flat list - no parent
}

int LazyFileModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_documents.size();
}

int LazyFileModel::columnCount(const QModelIndex&) const
{
    return ColCount;
}

QVariant LazyFileModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_documents.size())
        return {};

    const Document& doc = m_documents[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName: return doc.fileName;
        case ColSize: return formatSize(doc.fileSize);
        case ColTime: return formatTime(doc.modifiedTime);
        case ColType: return doc.fileExt.toUpper();
        }
    }

    if (role == Qt::DecorationRole && index.column() == ColName) {
        QFileInfo fi(doc.filePath);
        return m_iconProvider.icon(fi);
    }

    if (role == Qt::ToolTipRole && index.column() == ColName)
        return doc.filePath;

    if (role == Qt::UserRole)
        return QVariant::fromValue(doc.docId);

    return {};
}

QVariant LazyFileModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};

    switch (section) {
    case ColName: return tr("名称");
    case ColSize: return tr("大小");
    case ColTime: return tr("修改时间");
    case ColType: return tr("类型");
    }
    return {};
}

void LazyFileModel::sort(int column, Qt::SortOrder order)
{
    beginResetModel();

    std::sort(m_documents.begin(), m_documents.end(),
        [column, order](const Document& a, const Document& b) {
            bool lessThan = false;
            switch (column) {
            case ColName: lessThan = a.fileName.toLower() < b.fileName.toLower(); break;
            case ColSize: lessThan = a.fileSize < b.fileSize; break;
            case ColTime: lessThan = a.modifiedTime < b.modifiedTime; break;
            case ColType: lessThan = a.fileExt.toLower() < b.fileExt.toLower(); break;
            default: lessThan = a.docId < b.docId;
            }
            return order == Qt::AscendingOrder ? lessThan : !lessThan;
        });

    endResetModel();
}

const Document& LazyFileModel::documentAt(int row) const
{
    return m_documents[row];
}

int LazyFileModel::documentCount() const
{
    return m_documents.size();
}

QString LazyFileModel::formatSize(int64_t bytes)
{
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024LL * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}

QString LazyFileModel::formatTime(int64_t secsSinceEpoch)
{
    if (secsSinceEpoch <= 0) return "-";
    return QDateTime::fromSecsSinceEpoch(secsSinceEpoch).toString("yyyy-MM-dd HH:mm");
}
