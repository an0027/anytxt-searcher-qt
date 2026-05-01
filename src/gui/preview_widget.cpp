/*
 * preview_widget.cpp - 文档预览面板实现

实现文档内容渲染、元数据展示和关键词高亮。
 */

#include <QHeaderView>
#include <QScrollBar>
#include "gui/preview_widget.h"
#include "utils/file_utils.h"
#include "utils/string_utils.h"
#include <QDateTime>
#include <QDebug>

PreviewWidget::PreviewWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void PreviewWidget::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { background-color: #1e1e1e; border: 1px solid #555; }"
        "QTabBar::tab { background-color: #2d2d2d; color: #d4d4d4; "
        "padding: 6px 12px; border: 1px solid #555; border-bottom: none; }"
        "QTabBar::tab:selected { background-color: #1e1e1e; }");

    // Content tab
    m_contentView = new QTextEdit(this);
    m_contentView->setReadOnly(true);
    m_contentView->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #555; "
        "font-family: 'Consolas', 'monospace'; font-size: 12px; }");
    m_tabWidget->addTab(m_contentView, tr("内容"));

    // Metadata tab
    m_metadataView = new QTreeWidget(this);
    m_metadataView->setColumnCount(2);
    m_metadataView->setHeaderLabels({tr("属性"), tr("值")});
    m_metadataView->header()->setStretchLastSection(true);
    m_metadataView->setAlternatingRowColors(true);
    m_metadataView->setRootIsDecorated(false);
    m_metadataView->setStyleSheet(
        "QTreeWidget { background-color: #1e1e1e; color: #d4d4d4; "
        "alternate-background-color: #2d2d2d; border: 1px solid #555; }"
        "QTreeWidget::item:selected { background-color: #094771; }"
        "QHeaderView::section { background-color: #2d2d2d; color: #d4d4d4; "
        "border: 1px solid #555; padding: 4px; }");
    m_tabWidget->addTab(m_metadataView, tr("元数据"));

    // Statistics tab
    m_statsView = new QTextEdit(this);
    m_statsView->setReadOnly(true);
    m_statsView->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #555; "
        "font-family: 'Consolas', 'monospace'; font-size: 12px; }");
    m_tabWidget->addTab(m_statsView, tr("统计"));

    layout->addWidget(m_tabWidget, 1);

    // File info bar (at bottom)
    auto* infoBar = new QHBoxLayout();

    m_fileInfoLabel = new QLabel(this);
    m_fileInfoLabel->setStyleSheet(
        "font-weight: bold; color: #fff; padding: 4px; "
        "background-color: #2d2d2d; border: 1px solid #555; border-radius: 4px;");

    m_openFileBtn = new QPushButton(tr("打开文件"), this);
    m_openFileBtn->setStyleSheet(
        "QPushButton { padding: 4px 12px; border: 1px solid #555; color: #fff; "
        "border-radius: 4px; background-color: #3c3c3c; }"
        "QPushButton:hover { background-color: #4c4c4c; }");

    m_copyPathBtn = new QPushButton(tr("复制路径"), this);
    m_copyPathBtn->setStyleSheet(
        "QPushButton { padding: 4px 12px; border: 1px solid #555; color: #fff; "
        "border-radius: 4px; background-color: #3c3c3c; }"
        "QPushButton:hover { background-color: #4c4c4c; }");

    infoBar->addWidget(m_fileInfoLabel, 1);
    infoBar->addWidget(m_openFileBtn);
    infoBar->addWidget(m_copyPathBtn);

    layout->addLayout(infoBar);

    // Connections
    connect(m_openFileBtn, &QPushButton::clicked, this, &PreviewWidget::onOpenFile);
    connect(m_copyPathBtn, &QPushButton::clicked, this, &PreviewWidget::onCopyPath);
}

void PreviewWidget::setDocument(const Document& doc, const QString& highlightQuery)
{
    m_currentDoc = doc;

    // Update file info label
    QString fileInfo = QString("%1  |  %2  |  %3")
        .arg(doc.fileName)
        .arg(FileUtils::formatFileSize(doc.fileSize))
        .arg(QDateTime::fromSecsSinceEpoch(doc.modifiedTime)
                .toString("yyyy-MM-dd HH:mm:ss"));
    m_fileInfoLabel->setText(fileInfo);

    // Content tab - with line numbers + anchors + red text + yellow highlight
    QString content = doc.content.left(200000);
    QStringList lines = content.split('\n');
    int maxLines = qMin(lines.size(), 5000);

    QString html;
    html += QStringLiteral("<html><body style='background-color: #1e1e1e; color: #d4d4d4; "
                           "font-family: Consolas, monospace; font-size: 13px; margin: 0; padding: 0;'>");

    bool doHighlight = !highlightQuery.isEmpty();
    for (int i = 0; i < maxLines; ++i) {
        QString lineNum = QStringLiteral("%1").arg(i + 1, 4, 10, QChar(' '));
        html += QStringLiteral("<div id='para_%1' style='display: flex; line-height: 1.3;'>")
                .arg(i);
        html += QStringLiteral("<span style='color: #858585; user-select: none; padding-right: 8px; width: 36px; text-align: right;'") +
                QStringLiteral(">%1</span>").arg(lineNum);

        if (lines[i].isEmpty()) {
            html += QStringLiteral("<span style='color: #d4d4d4;'>&nbsp;</span>");
        } else if (doHighlight) {
            QString hl = StringUtils::highlightText(lines[i], highlightQuery);
            html += QStringLiteral("<span style='color: #d4d4d4;'>%1</span>").arg(hl);
        } else {
            html += QStringLiteral("<span style='color: #d4d4d4;'>%1</span>")
                    .arg(lines[i].toHtmlEscaped());
        }

        html += QStringLiteral("</div>");
    }

    if (lines.size() > maxLines) {
        html += QStringLiteral("<div style='color: #888; padding-top: 8px;'>"
                                "... 显示 %1 / %2 行</div>")
                .arg(maxLines).arg(lines.size());
    }

    html += QStringLiteral("</body></html>");
    m_contentView->setHtml(html);
    m_contentView->verticalScrollBar()->setValue(0);

    // Metadata tab
    populateMetadata(doc);

    // Statistics tab
    populateStatistics(doc);

    // Enable buttons
    m_openFileBtn->setEnabled(!doc.filePath.isEmpty());
    m_copyPathBtn->setEnabled(!doc.filePath.isEmpty());

    // Switch to content tab
    m_tabWidget->setCurrentIndex(0);
}

void PreviewWidget::scrollToParagraph(int paraIndex)
{
    if (paraIndex < 0) return;
    QString anchor = QStringLiteral("para_%1").arg(paraIndex);
    m_contentView->scrollToAnchor(anchor);
}

void PreviewWidget::clear()
{
    m_currentDoc = Document();
    m_fileInfoLabel->clear();
    m_contentView->clear();
    m_metadataView->clear();
    m_statsView->clear();
    m_openFileBtn->setEnabled(false);
    m_copyPathBtn->setEnabled(false);
}

void PreviewWidget::populateMetadata(const Document& doc)
{
    m_metadataView->clear();

    auto addItem = [this](const QString& key, const QString& value) {
        auto* item = new QTreeWidgetItem(m_metadataView);
        item->setText(0, key);
        item->setText(1, value);
    };

    addItem(tr("文件名"), doc.fileName);
    addItem(tr("路径"), doc.filePath);
    addItem(tr("大小"), FileUtils::formatFileSize(doc.fileSize));
    addItem(tr("修改时间"),
        QDateTime::fromSecsSinceEpoch(doc.modifiedTime).toString("yyyy-MM-dd HH:mm:ss"));
    addItem(tr("文件类型"), doc.mimeType);
    addItem(tr("扩展名"), doc.fileExt);
    addItem(tr("文档ID"), QString::number(doc.docId));
    addItem(tr("相关度"), QString("%1%").arg(doc.percent));
    addItem(tr("排名"), QString::number(doc.rank));

    // Custom metadata
    if (!doc.metadata.isEmpty()) {
        for (auto it = doc.metadata.constBegin(); it != doc.metadata.constEnd(); ++it) {
            if (it.key() == "mimeType" || it.key() == "fileSize" ||
                it.key() == "modifiedTime" || it.key() == "fileExt") {
                continue; // skip already displayed metadata
            }
            addItem(it.key(), it.value());
        }
    }

    // Resize columns
    m_metadataView->resizeColumnToContents(0);
}

void PreviewWidget::populateStatistics(const Document& doc)
{
    QString stats;
    stats += tr("=== 文档统计 ===\n\n");
    stats += tr("字符数: %1\n").arg(doc.content.length());
    stats += tr("单词数: %1\n").arg(doc.content.split(QRegularExpression("\\s+"),
                                         Qt::SkipEmptyParts).size());
    stats += tr("行数: %1\n").arg(doc.content.count('\n') + 1);
    stats += tr("文件大小: %1\n").arg(FileUtils::formatFileSize(doc.fileSize));
    stats += tr("相关度分数: %1\n\n").arg(doc.relevance, 0, 'f', 2);

    stats += tr("=== 元数据统计 ===\n\n");
    stats += tr("元数据项数: %1\n").arg(doc.metadata.size());

    m_statsView->setPlainText(stats);
}

void PreviewWidget::onOpenFile()
{
    if (!m_currentDoc.filePath.isEmpty()) {
        emit openFile(m_currentDoc);
    }
}

void PreviewWidget::onCopyPath()
{
    if (!m_currentDoc.filePath.isEmpty()) {
        emit copyPath(m_currentDoc.filePath);
    }
}
