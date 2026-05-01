/*
 * file_panel.h - 文件列表面板（左侧）

功能说明：显示索引目录中的文件列表，支持按文件名/时间/相关性排序。
点击文件触发预览。
 */

#ifndef ANYTXT_FILE_PANEL_H
#define ANYTXT_FILE_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
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

private slots:
    void onSortChanged(int index);
    void onTypeFilterChanged();

private:
    void setupUI();
    void applySort();
    int sortValue(const Document& doc, int mode) const;

    QListWidget* m_list;
    QLabel* m_countLabel;
    QListWidget* m_typeList;
    QComboBox* m_sortCombo;
    QPushButton* m_sortOrderBtn;
    QVector<Document> m_documents;
    QSet<int64_t> m_matchIds;
    bool m_hasActiveSearch = false;
    bool m_sortReverse = false;
};

#endif // ANYTXT_FILE_PANEL_H
