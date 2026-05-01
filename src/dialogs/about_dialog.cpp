/*
 * about_dialog.cpp - 关于对话框实现

实现关于对话框的 UI 布局。
 */

#include "about_dialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("关于 AnyTXT Searcher");
    setFixedSize(400, 300);

    auto* layout = new QVBoxLayout(this);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel("AnyTXT Searcher");
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* versionLabel = new QLabel("版本 1.0.0");
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    auto* descLabel = new QLabel(
        "一个强大的桌面全文搜索工具\n"
        "基于 Qt5 + Xapian 构建\n\n"
        "支持格式: TXT, PDF, DOCX 等\n"
        "支持中文全文搜索"
    );
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addStretch();

    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto* closeBtn = new QPushButton("关闭");
    closeBtn->setFixedWidth(80);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnLayout->addWidget(closeBtn);
    layout->addLayout(btnLayout);
}
