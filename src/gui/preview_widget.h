/*
 * preview_widget.h - 文档预览面板

功能说明：以选项卡形式展示文档内容、元数据和统计信息，
支持搜索关键词高亮。
 */

#ifndef ANYTXT_PREVIEW_WIDGET_H
#define ANYTXT_PREVIEW_WIDGET_H

#include "core/document.h"
#include <QWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

class PreviewWidget : public QWidget {
    Q_OBJECT
public:
    explicit PreviewWidget(QWidget* parent = nullptr);
    ~PreviewWidget() override = default;

    void setDocument(const Document& doc, const QString& highlightQuery = QString());
    void scrollToParagraph(int paraIndex);
    void clear();
    Document currentDocument() const { return m_currentDoc; }

signals:
    void openFile(const Document& doc);
    void copyPath(const QString& path);

private slots:
    void onOpenFile();
    void onCopyPath();

private:
    void setupUI();
    void populateMetadata(const Document& doc);
    void populateStatistics(const Document& doc);

    QLabel* m_fileInfoLabel;
    QPushButton* m_openFileBtn;
    QPushButton* m_copyPathBtn;
    QTabWidget* m_tabWidget;
    QTextEdit* m_contentView;
    QTreeWidget* m_metadataView;
    QTextEdit* m_statsView;

    Document m_currentDoc;
};

#endif // ANYTXT_PREVIEW_WIDGET_H
