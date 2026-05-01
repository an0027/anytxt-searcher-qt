/*
 * help_dialog.h - 使用手册对话框

功能说明：显示 AnyTXT Searcher 的完整使用说明。
 */

#ifndef ANYTXT_HELP_DIALOG_H
#define ANYTXT_HELP_DIALOG_H

#include <QDialog>

class HelpDialog : public QDialog {
    Q_OBJECT
public:
    explicit HelpDialog(QWidget* parent = nullptr);
    ~HelpDialog() override = default;
};

#endif // ANYTXT_HELP_DIALOG_H
