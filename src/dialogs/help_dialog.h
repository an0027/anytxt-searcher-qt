#ifndef ANYTXT_HELP_DIALOG_H
#define ANYTXT_HELP_DIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QTextBrowser>
#include <QPushButton>
#include <QSplitter>

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override = default;
private slots:
    void onTreeItemClicked(QTreeWidgetItem* item, int column);
    void openChmHelp();
private:
    void setupUi();
    void populateTree();
    void loadSection(const QString& sectionId);
    QString getFullHtml(const QString& bodyHtml) const;
    QString getHtmlContent(const QString& sectionId) const;
    QSplitter* m_splitter;
    QTreeWidget* m_tree;
    QTextBrowser* m_browser;
    QPushButton* m_chmBtn;
};
#endif
