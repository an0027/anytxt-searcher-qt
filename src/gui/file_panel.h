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
    void excludePath(const QString& filePath);

private slots:
    void onSortChanged(int index);

private:
    void setupUI();
    void applySort();

    QLabel* m_countLabel;
    QComboBox* m_sortCombo;
    QPushButton* m_sortOrderBtn;
    QListWidget* m_list;

    QVector<Document> m_documents;
    QSet<int64_t> m_matchIds;
    bool m_hasActiveSearch = false;
    int m_sortMode = 1; // 0=name, 1=relevance, 2=date, 3=size
    bool m_sortReverse = false;
};

#endif
