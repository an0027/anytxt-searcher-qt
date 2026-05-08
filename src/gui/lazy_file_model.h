/*
 * lazy_file_model.h - 虚拟滚动文件列表模型
 *
 * QAbstractItemModel 实现，持有 QVector<Document> 但不创建任何 UI 对象。
 * QTreeView 通过此模型按需获取数据，支持排序和文件图标。
 */

#ifndef ANYTXT_LAZY_FILE_MODEL_H
#define ANYTXT_LAZY_FILE_MODEL_H

#include <QAbstractItemModel>
#include <QFileIconProvider>
#include <QVector>
#include <QDateTime>
#include "core/document.h"

class LazyFileModel : public QAbstractItemModel {
    Q_OBJECT
public:
    enum Column { ColName = 0, ColSize, ColTime, ColType, ColCount };

    explicit LazyFileModel(QObject* parent = nullptr);
    ~LazyFileModel() override;

    void setDocuments(const QVector<Document>& docs);
    void clear();

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    // Sorting
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    // Access document by row
    const Document& documentAt(int row) const;
    int documentCount() const;

private:
    static QString formatSize(int64_t bytes);
    static QString formatTime(int64_t secsSinceEpoch);

    QVector<Document> m_documents;
    mutable QFileIconProvider m_iconProvider;
};

#endif // ANYTXT_LAZY_FILE_MODEL_H
