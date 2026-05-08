#include "help_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QApplication>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>
#include <QLabel>
#include <QFont>

HelpDialog::HelpDialog(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("AnyTXT Searcher 使用手册"));
    resize(850, 640);
    setMinimumSize(640, 480);
    setupUi();
    populateTree();
}

void HelpDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* titleLabel = new QLabel(tr("AnyTXT Searcher 使用手册"));
    QFont tf = titleLabel->font();
    tf.setPointSize(15);
    tf.setBold(true);
    titleLabel->setFont(tf);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    m_splitter = new QSplitter(Qt::Horizontal, this);

    m_tree = new QTreeWidget(m_splitter);
    m_tree->setHeaderHidden(true);
    m_tree->setMinimumWidth(180);
    m_tree->setMaximumWidth(280);
    connect(m_tree, &QTreeWidget::itemClicked, this, &HelpDialog::onTreeItemClicked);

    m_browser = new QTextBrowser(m_splitter);
    m_browser->setOpenExternalLinks(true);

    m_splitter->addWidget(m_tree);
    m_splitter->addWidget(m_browser);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 3);
    m_splitter->setSizes({220, 630});
    mainLayout->addWidget(m_splitter, 1);

    auto* btnLayout = new QHBoxLayout();
    m_chmBtn = new QPushButton(tr("打开 CHM 帮助文件"));
    m_chmBtn->setToolTip(tr("如果同目录存在 AnyTXT-Searcher.chm 则打开"));
    connect(m_chmBtn, &QPushButton::clicked, this, &HelpDialog::openChmHelp);
    btnLayout->addWidget(m_chmBtn);
    btnLayout->addStretch();

    auto* closeBtn = new QPushButton(tr("关闭"));
    closeBtn->setFixedWidth(80);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    if (m_tree->topLevelItemCount() > 0) {
        m_tree->setCurrentItem(m_tree->topLevelItem(0));
        loadSection("quickstart");
    }
}

void HelpDialog::populateTree()
{
    auto add = [this](const QString& title, const QString& id) {
        auto* item = new QTreeWidgetItem(m_tree, {title});
        item->setData(0, Qt::UserRole, id);
    };
    add(tr("快速入门"), "quickstart");
    add(tr("搜索技巧"), "search");
    add(tr("格式支持"), "formats");
    add(tr("索引管理"), "indexing");
    add(tr("导入导出"), "importexport");
    add(tr("快捷键"), "shortcuts");
    add(tr("常见问题"), "faq");
    m_tree->expandAll();
}

void HelpDialog::onTreeItemClicked(QTreeWidgetItem* item, int)
{
    if (!item) return;
    QString id = item->data(0, Qt::UserRole).toString();
    if (!id.isEmpty()) loadSection(id);
}

void HelpDialog::loadSection(const QString& sectionId)
{
    m_browser->setHtml(getFullHtml(getHtmlContent(sectionId)));
}

void HelpDialog::openChmHelp()
{
    QString appDir = QApplication::applicationDirPath();
    QString chmPath = appDir + "/AnyTXT-Searcher.chm";
    if (QFileInfo::exists(chmPath))
        QDesktopServices::openUrl(QUrl::fromLocalFile(chmPath));
}

QString HelpDialog::getFullHtml(const QString& bodyHtml) const
{
    return QStringLiteral(
        "<html><body style='font-family: sans-serif;'>%1</body></html>"
    ).arg(bodyHtml);
}

QString HelpDialog::getHtmlContent(const QString& sectionId) const
{
    if (sectionId == "quickstart")
        return QStringLiteral(
            "<h2>快速入门</h2><p>AnyTXT Searcher 是一款高性能桌面全文搜索工具。</p>"
            "<ol><li>输入关键词按回车搜索</li>"
            "<li>支持 &amp; (AND) | (OR) - (排除) 运算符</li>"
            "<li>支持 title: / file: 域搜索</li></ol>");
    if (sectionId == "search")
        return QStringLiteral(
            "<h2>搜索技巧</h2><ul>"
            "<li><code>&amp;</code> AND：<code>报告 &amp; 2024</code></li>"
            "<li><code>|</code> OR：<code>报告 | 汇总</code></li>"
            "<li><code>-</code> 排除：<code>报告 -草案</code></li>"
            "<li><code>title:xxx</code> 标题搜索</li>"
            "<li><code>file:xxx</code> 文件名搜索</li></ul>");
    if (sectionId == "formats")
        return QStringLiteral(
            "<h2>格式支持</h2><table border=1 cellpadding=6 style='border-collapse:collapse'>"
            "<tr><td>纯文本</td><td>.txt .md .csv .html .xml .json .py .js .cpp 等</td></tr>"
            "<tr><td>Word</td><td>.docx .docm .doc</td></tr>"
            "<tr><td>Excel</td><td>.xlsx .xlsm</td></tr>"
            "<tr><td>PowerPoint</td><td>.pptx .pptm</td></tr>"
            "<tr><td>PDF</td><td>.pdf</td></tr>"
            "<tr><td>电子邮件</td><td>.eml</td></tr>"
            "<tr><td>RTF</td><td>.rtf</td></tr>"
            "<tr><td>电子书</td><td>.epub</td></tr>"
            "<tr><td>WPS 系列</td><td>.wps .et .dps</td></tr>"
            "</table><p>共 15 种文件格式。</p>");
    if (sectionId == "indexing")
        return QStringLiteral(
            "<h2>索引管理</h2>"
            "<p><b>重建索引</b>：工具栏按钮，重新扫描构建索引。</p>"
            "<p><b>优化索引</b>：压缩索引数据库。</p>"
            "<p><b>偏好设置</b> → <b>索引</b>：批量大小和拼写检查。</p>");
    if (sectionId == "importexport")
        return QStringLiteral(
            "<h2>导入导出</h2>"
            "<p><b>导入文档</b>：工具栏按钮，批量导入。</p>"
            "<p><b>导出结果</b>：导出搜索结果为 CSV 或文本文件。</p>");
    if (sectionId == "shortcuts")
        return QStringLiteral(
            "<h2>快捷键</h2>"
            "<table border=1 cellpadding=6 style='border-collapse:collapse'>"
            "<tr><td>聚焦搜索框</td><td>Ctrl+F</td></tr>"
            "<tr><td>导入文档</td><td>Ctrl+I</td></tr>"
            "<tr><td>导出结果</td><td>Ctrl+E</td></tr>"
            "<tr><td>重建索引</td><td>Ctrl+N</td></tr>"
            "<tr><td>退出</td><td>Ctrl+Q</td></tr></table>");
    if (sectionId == "faq")
        return QStringLiteral(
            "<h2>常见问题</h2>"
            "<p><b>索引速度慢？</b>增大偏好设置中的批量大小。</p>"
            "<p><b>搜不到文件？</b>确认文件格式被支持，尝试重建索引。</p>"
            "<p><b>界面显示异常？</b>尝试切换主题。</p>");
    return QStringLiteral("<h2>未知章节</h2>");
}
