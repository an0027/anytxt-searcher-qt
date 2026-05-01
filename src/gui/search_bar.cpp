#include "gui/search_bar.h"
#include <QLabel>
#include <QDebug>

SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void SearchBar::setHistory(const QStringList& history)
{
    m_history = history;
    updateCompleter();
}

void SearchBar::updateCompleter()
{
    if (!m_completer) {
        m_completer = new QCompleter(this);
        m_historyModel = new QStringListModel(this);
        m_completer->setModel(m_historyModel);
        m_completer->setCaseSensitivity(Qt::CaseInsensitive);
        m_completer->setMaxVisibleItems(10);
        m_completer->setCompletionMode(QCompleter::PopupCompletion);
        m_searchInput->setCompleter(m_completer);
    }
    m_historyModel->setStringList(m_history);
}

void SearchBar::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    auto* row = new QHBoxLayout();
    row->setSpacing(4);

    // Scope selector
    m_scopeCombo = new QComboBox(this);
    m_scopeCombo->addItem(tr("全文"), "all");
    m_scopeCombo->addItem(tr("文件名"), "file");
    m_scopeCombo->addItem(tr("标题"), "title");
    m_scopeCombo->setCurrentIndex(0);
    m_scopeCombo->setMinimumHeight(36);
    m_scopeCombo->setFixedWidth(80);
    m_scopeCombo->setStyleSheet(
        "QComboBox { background-color: #3c3c3c; color: #d4d4d4; "
        "border: 1px solid #555; border-radius: 4px; padding: 4px 8px; }"
        "QComboBox::drop-down { border: none; width: 20px; }"
        "QComboBox QAbstractItemView { background-color: #3c3c3c; "
        "color: #d4d4d4; selection-background-color: #094771; }");

    // Search input
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("输入搜索词，& 为 AND，| 为 OR，- 为排除..."));
    m_searchInput->setClearButtonEnabled(true);
    m_searchInput->setMinimumHeight(36);
    QFont f = m_searchInput->font();
    f.setPointSize(f.pointSize() + 2);
    m_searchInput->setFont(f);

    // Search button
    m_searchButton = new QPushButton(tr("搜索"), this);
    m_searchButton->setMinimumHeight(36);
    m_searchButton->setMinimumWidth(70);
    m_searchButton->setStyleSheet(
        "QPushButton { background-color: #1976D2; color: white; border: none; "
        "border-radius: 4px; padding: 6px 16px; font-size: 14px; }"
        "QPushButton:hover { background-color: #1565C0; }"
        "QPushButton:pressed { background-color: #0D47A1; }");

    row->addWidget(m_scopeCombo);
    row->addWidget(m_searchInput, 1);
    row->addWidget(m_searchButton);
    mainLayout->addLayout(row);

    // Connections
    connect(m_searchButton, &QPushButton::clicked, this, &SearchBar::onSearchClicked);
    connect(m_searchInput, &QLineEdit::returnPressed, this, &SearchBar::onReturnPressed);
    connect(m_scopeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { emit scopeChanged(m_scopeCombo->currentData().toString()); });
}

void SearchBar::onSearchClicked()
{
    onReturnPressed();
}

void SearchBar::onReturnPressed()
{
    QString q = m_searchInput->text().trimmed();
    if (q.isEmpty()) return;

    QVariantMap options;
    options["matchType"] = QString("and");
    options["scope"] = m_scopeCombo->currentData().toString();
    emit search(q, options);
}

void SearchBar::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Escape) {
        m_searchInput->clear();
    }
    QWidget::keyPressEvent(event);
}

QString SearchBar::query() const
{
    return m_searchInput->text().trimmed();
}

void SearchBar::setQuery(const QString& query)
{
    m_searchInput->setText(query);
}

void SearchBar::setScopeCombo(const QString& scope)
{
    for (int i = 0; i < m_scopeCombo->count(); ++i) {
        if (m_scopeCombo->itemData(i).toString() == scope) {
            m_scopeCombo->setCurrentIndex(i);
            return;
        }
    }
}

QString SearchBar::scope() const
{
    return m_scopeCombo->currentData().toString();
}

void SearchBar::focusSearch()
{
    m_searchInput->setFocus();
    m_searchInput->selectAll();
}
