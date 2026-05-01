/*
 * search_bar.h - 搜索栏组件

功能说明：搜索输入框，支持高级搜索模式切换，
提供搜索按钮和搜索选项配置。
 */

#ifndef ANYTXT_SEARCH_BAR_H
#define ANYTXT_SEARCH_BAR_H

#include <QWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QKeyEvent>

class SearchBar : public QWidget {
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);
    ~SearchBar() override = default;

    QString query() const;
    void setQuery(const QString& query);
    void setScopeCombo(const QString& scope);
    QString scope() const;
    void focusSearch();

signals:
    void search(const QString& query, const QVariantMap& options);
    void scopeChanged(const QString& scope);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSearchClicked();
    void onReturnPressed();

private:
    void setupUI();

    QLineEdit* m_searchInput;
    QPushButton* m_searchButton;
    QComboBox* m_scopeCombo;
};

#endif // ANYTXT_SEARCH_BAR_H
