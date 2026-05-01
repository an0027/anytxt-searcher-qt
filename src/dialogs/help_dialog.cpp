/*
 * help_dialog.cpp - 使用手册对话框实现
 */

#include "help_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextBrowser>
#include <QPushButton>
#include <QLabel>
#include <QFont>

HelpDialog::HelpDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("AnyTXT Searcher 使用手册"));
    resize(680, 520);
    setMinimumSize(520, 400);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(tr("📖 使用手册"));
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto* browser = new QTextBrowser(this);
    browser->setOpenExternalLinks(true);
    browser->setStyleSheet(
        "QTextBrowser { background-color: #fafafa; border: 1px solid #ddd; "
        "border-radius: 4px; padding: 12px; font-size: 14px; line-height: 1.6; }");

    QString html = R"(<html><body style="font-family: 'Microsoft YaHei', 'Segoe UI', sans-serif;">

<h2>快速入门</h2>
<p>AnyTXT Searcher 是一款高性能桌面全文搜索工具，支持中文分词和多种文档格式。</p>
<hr>

<h3>1. 搜索</h3>
<ul>
  <li>在顶部搜索框中输入关键词，按 <b>回车</b> 或点击 <b>搜索按钮</b> 开始搜索</li>
  <li>支持逻辑运算符：<code>&amp;</code>（AND）、<code>|</code>（OR）、<code>-</code>（排除）</li>
  <li>示例：<code>报告 &amp; 2024 -草案</code> 表示搜索包含"报告"和"2024"但不含"草案"的内容</li>
  <li>通过下拉框切换搜索范围：<b>全文</b> / <b>文件名</b> / <b>标题</b></li>
</ul>

<h3>2. 搜索结果</h3>
<ul>
  <li>结果列表按列显示：名称、路径、大小、修改时间、类型、相关性</li>
  <li>点击列上方排序下拉框可按 <b>相关性</b> / <b>日期</b> / <b>大小</b> / <b>名称</b> 排序</li>
  <li>点击排序切换按钮 <b>↑/↓</b> 切换升序/降序</li>
  <li>底部分页导航，支持翻页浏览</li>
  <li>右键菜单支持：打开文件、打开所在文件夹、复制路径、复制文件名</li>
</ul>

<h3>3. 预览与段落跳转</h3>
<ul>
  <li>选中结果后，右侧上方显示文件内容预览</li>
  <li>右侧下方显示 <b>命中段落</b> 列表，点击可定位到对应段落</li>
</ul>

<h3>4. 侧边栏文件浏览</h3>
<ul>
  <li>左侧面板按目录树展示已索引的文件</li>
  <li>点击文件直接查看详情</li>
</ul>

<h3>5. 筛选条件</h3>
<ul>
  <li>可按 <b>文件类型</b>：PDF、DOCX、TXT、图片、代码、压缩包</li>
  <li>可按 <b>修改时间</b>：今天、一周内、一月内、一年内</li>
  <li>可按 <b>文件大小</b>：小于1MB、1-100MB、大于100MB</li>
  <li>点击 <b>清除筛选</b> 恢复全部结果</li>
</ul>

<h3>6. 索引管理</h3>
<ul>
  <li><b>工具 → 智能索引设置</b>：配置自动监控文件夹，新文件自动编入索引</li>
  <li><b>工具 → 重建索引</b>：重新扫描所有文档，构建全新索引</li>
  <li><b>工具 → 优化索引</b>：压缩优化索引数据库，提升搜索性能</li>
</ul>

<h3>7. 导入导出</h3>
<ul>
  <li><b>文件 → 导入文档</b>：批量导入文档到索引库</li>
  <li><b>文件 → 导出结果</b>：将当前搜索结果导出到文件</li>
</ul>

<h3>8. 界面设置</h3>
<ul>
  <li><b>视图 → 切换主题</b>：在亮色/暗色主题之间切换</li>
  <li>窗口大小和状态会自动保存，下次启动恢复</li>
</ul>

<h3>9. 快捷键</h3>
<table border="1" cellpadding="6" cellspacing="0" style="border-collapse:collapse; border:1px solid #ccc;">
<tr style="background:#f0f0f0;"><th>操作</th><th>快捷键</th></tr>
<tr><td>新建索引</td><td><code>Ctrl+N</code></td></tr>
<tr><td>导入文档</td><td><code>Ctrl+I</code></td></tr>
<tr><td>导出结果</td><td><code>Ctrl+E</code></td></tr>
<tr><td>退出</td><td><code>Ctrl+Q</code></td></tr>
<tr><td>聚焦搜索框</td><td><code>Ctrl+F</code></td></tr>
</table>

<hr>
<p style="color:#888; font-size:12px; text-align:center;">
AnyTXT Searcher v1.0.0 &mdash; 基于 Qt + Xapian 构建
</p>

</body></html>
)";

    browser->setHtml(html);
    layout->addWidget(browser, 1);

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* closeBtn = new QPushButton(tr("关闭"));
    closeBtn->setFixedWidth(80);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
}
