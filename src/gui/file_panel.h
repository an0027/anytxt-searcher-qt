/*
 * file_panel.h - 文件列表面板 (Windows 资源管理器风格)
 */

#ifndef ANYTXT_FILE_PANEL_H
#define ANYTXT_FILE_PANEL_H

#include <QWidget>
#include <QTreeWidget>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <memory>
#include "core/document.h"

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() override = default;

    void setFiles(const QVector<Document>& docs);
    void setMatchIds(const QSet<int64_t>& matchDocIds);
    void clear();

signals:
    void fileSelected(const Document& doc);
    void excludePath(const QString& filePath);

private slots:
    void onTypeFilterChanged(int index);
    void onHeaderClicked(int logicalIndex);

private:
    void setupUI();
    void applySort();
    QTreeWidgetItem* createFileRow(const Document& doc, int idx);
    int currentSortColumn() const;

    QLabel* m_countLabel;
    QComboBox* m_typeCombo;
    QTreeWidget* m_tree;

    QVector<Document> m_documents;
    QSet<int64_t> m_matchIds;
    bool m_hasActiveSearch = false;
    int m_sortColumn = 0; // 0=name, 1=date, 2=size, 3=type
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
};

#endif // ANYTXT_FILE_PANEL_H
