/*
 * preferences_dialog.h - 偏好设置对话框
 */

#ifndef ANYTXT_PREFERENCES_DIALOG_H
#define ANYTXT_PREFERENCES_DIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QComboBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QPushButton>
#include <QColor>
#include <QLineEdit>
#include <memory>

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

    QString defaultScope() const;
    int pageSize() const;
    bool autoLoadLastSearch() const;
    bool darkTheme() const;
    QColor highlightColor() const;
    int batchSize() const;
    bool enableSpelling() const;
    QString webhookUrl() const;

private slots:
    void onApply();
    void onPickColor();

private:
    QWidget* createNotificationTab();
    void setupUI();
    void loadCurrentSettings();
    QWidget* createGeneralTab();
    QWidget* createDisplayTab();
    QWidget* createIndexTab();

    QTabWidget* m_tabWidget;

    // General
    QComboBox* m_scopeCombo;
    QSpinBox* m_pageSizeSpin;
    QCheckBox* m_autoLoadCheck;

    // Display
    QCheckBox* m_darkThemeCheck;
    QPushButton* m_colorBtn;
    QColor m_selectedColor;

    // Notification
    QLineEdit* m_webhookUrlEdit;

    // Index
    QSpinBox* m_batchSizeSpin;
    QCheckBox* m_spellingCheck;
};

#endif // ANYTXT_PREFERENCES_DIALOG_H
