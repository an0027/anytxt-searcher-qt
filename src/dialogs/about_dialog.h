/*
 * about_dialog.h - 关于对话框

功能说明：显示应用版本信息和版权声明。
 */

#ifndef ANYTXT_ABOUT_DIALOG_H
#define ANYTXT_ABOUT_DIALOG_H

#include <QDialog>

class AboutDialog : public QDialog {
    Q_OBJECT
public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog() override = default;
};

#endif // ANYTXT_ABOUT_DIALOG_H
