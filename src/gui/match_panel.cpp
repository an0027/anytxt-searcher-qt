#include "gui/match_panel.h"
#include <QDebug>

MatchPanel::MatchPanel(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void MatchPanel::setupUI()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    m_label = new QLabel(tr("命中段落"), this);
    m_label->setStyleSheet("font-weight: bold; padding: 6px 8px;");

    m_list = new QListWidget(this);
    m_list->setStyleSheet(
        "QListWidget { background: transparent; border: none; font-size: 12px; }"
        "QListWidget::item { padding: 8px; border-bottom: 1px solid #e0e0e0; }"
        "QListWidget::item:selected { background-color: #E8F0FE; }"
        "QListWidget::item:hover { background-color: #f0f0f0; }");
    m_list->setWordWrap(true);
    m_list->setSpacing(1);

    layout->addWidget(m_label);
    layout->addWidget(m_list, 1);

    connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        if (!item) return;
        int paraIdx = item->data(Qt::UserRole).toInt();
        if (paraIdx >= 0) {
            emit paragraphClicked(paraIdx);
        }
    });
}

void MatchPanel::setMatches(const QVector<MatchItem>& items, int totalHits)
{
    m_list->clear();
    for (const auto& mi : items) {
        auto* item = new QListWidgetItem();
        // Format: line number + snippet preview
        QString display = QStringLiteral("¶ %1  %2")
            .arg(mi.paragraphIndex + 1)
            .arg(mi.snippet.left(200).simplified());
        item->setText(display);
        item->setData(Qt::UserRole, mi.paragraphIndex);
        item->setToolTip(mi.snippet);
        m_list->addItem(item);
    }
    m_label->setText(QString(tr("命中段落 (%1)")).arg(totalHits));
}

void MatchPanel::clear()
{
    m_list->clear();
    m_label->setText(tr("命中段落"));
}
