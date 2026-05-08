/*
 * file_panel.h - 文件浏览面板（QTreeView + 虚拟滚动 Model）
 */

#ifndef ANYTXT_FILE_PANEL_H
#define ANYTXT_FILE_PANEL_H

#include <QWidget>
#include <QTreeView>
#include <QLabel>
#include <QVBoxLayout>
#include <QHeaderView>
#include <memory>
#include "core/document.h"

class LazyFileModel;

class FilePanel : public QWidget {
    Q_OBJECT
public:
    explicit FilePanel(QWidget* parent = nullptr);
    ~FilePanel() override = default;

    void setFiles(const QVector<Document>& docs);
    void setMatchIds(const QSet<int64_t>& matchDocIds) { m_matchIds = matchDocIds; }
    void clear();

signals:
    void fileSelected(const Document& doc);
    void excludePath(const QString& filePath);

private:
    void setupUI();

    QLabel* m_countLabel;
    QTreeView* m_view;
    LazyFileModel* m_model;
    QSet<int64_t> m_matchIds;
};

#endif
