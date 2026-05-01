/*
 * results_widget.h - 搜索结果列表

功能说明：以表格形式展示搜索结果，支持排序、
分页、右键菜单、文件打开等交互操作。
 */

#ifndef ANYTXT_RESULTS_WIDGET_H
#define ANYTXT_RESULTS_WIDGET_H

#include "core/document.h"
#include <QWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPoint>
#include <QMouseEvent>

class ResultsWidget : public QWidget {
    Q_OBJECT
public:
    explicit ResultsWidget(QWidget* parent = nullptr);
    ~ResultsWidget() override = default;

    void setResults(const QVector<Document>& documents, int totalResults);
    void clear();
    Document selectedDocument() const;
    int currentPage() const { return m_currentPage; }
    int pageSize() const { return m_pageSize; }
    int totalResults() const { return m_totalResults; }

signals:
    void resultSelected(const Document& document);
    void openFile(const Document& document);
    void copyPath(const QString& path);
    void pageChanged(int page);
    void sortChanged(const QString& sortBy, bool reverse);
    void excludeFile(const QString& filePath);

private slots:
    void onItemSelectionChanged();
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onCustomContextMenu(const QPoint& pos);
    void onPreviousPage();
    void onNextPage();
    void onSortComboChanged(int index);

private:
    void setupUI();
    void updatePagination();
    QTreeWidgetItem* createItem(const Document& doc, int row);

    // Drag support
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    QPoint m_dragStartPos;

    QTreeWidget* m_tree;
    QLabel* m_resultsCountLabel;
    QComboBox* m_sortCombo;
    QPushButton* m_sortOrderBtn;
    QPushButton* m_prevPageBtn;
    QPushButton* m_nextPageBtn;
    QLabel* m_pageLabel;

    QVector<Document> m_documents;
    int m_currentPage = 1;
    int m_pageSize = 50;
    int m_totalResults = 0;
    bool m_sortReverse = false;
};

#endif // ANYTXT_RESULTS_WIDGET_H
