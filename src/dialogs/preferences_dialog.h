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
#include <QStringList>
#include <memory>

class PreferencesDialog : public QDialog {
    Q_OBJECT
public:
    explicit PreferencesDialog(QWidget* parent = nullptr);
    ~PreferencesDialog() override = default;

    void setThemeList(const QStringList& themeNames, const QString& currentTheme);
    QString defaultScope() const;
    int pageSize() const;
    bool autoLoadLastSearch() const;
    QString themeName() const;
    QColor highlightColor() const;
    int batchSize() const;
    bool enableSpelling() const;

private slots:
    void onApply();
    void onPickColor();

private:
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
    QComboBox* m_themeCombo;
    QPushButton* m_colorBtn;
    QColor m_selectedColor;

    // Index
    QSpinBox* m_batchSizeSpin;
    QCheckBox* m_spellingCheck;
};

#endif // ANYTXT_PREFERENCES_DIALOG_H
