// ============================================================================
// main_window.h 鈥?涓荤獥鍙ｅ畾涔?
// ============================================================================
#ifndef ANYTXT_MAIN_WINDOW_H
#define ANYTXT_MAIN_WINDOW_H

#include <QMainWindow>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QStatusBar>
#include <QSettings>
#include <QToolBar>
#include <QMenuBar>
#include <QMutex>
#include <QFutureWatcher>
#include <QMenu>
#include <QAction>
#include <QSystemTrayIcon>
#include <QSet>
#include "core/notification_manager.h"
#include <QMap>
#include <memory>
#include "core/document.h"

class IndexConfig;
class XapianDatabase;
class XapianIndexer;
class XapianSearcher;
class ParserManager;
class FileWatcher;
class IndexQueue;

class SearchBar;
class ResultsWidget;
class PreviewWidget;
class FilePanel;
class FilterPanel;
class MatchPanel;
class ImportDialog;
class ExportDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(const QString& indexPath = QString(), QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onSearch(const QString& query, const QVariantMap& options);
    void onSearchFinished();
    void onResultSelected(const Document& doc);
    void onOpenFile(const Document& doc);
    void onCopyPath(const QString& path);
    void onImportRequested();
    void onExportRequested();
    void onReindex();
    void onOptimize();
    void onToggleTheme();
    void onToggleSidebar();
    void onHelp();
    void onAbout();
    void onPreferences();
    void onSortChanged(const QString& sortBy, bool reverse);
    void onWatchSettings();
    void onAddWatchFolder();
    void onFilesChanged(const QStringList& newFiles, const QStringList& modifiedFiles, const QStringList& deletedFiles);
    void onQueueProgress(int indexed, int total, const QString& currentFile);
    void onQueueFinished(int indexed, int failed);

private:
    void setupMenuBar();
    void setupToolBar();
    void setupCentralWidget();
    void setupStatusBar();
    void setupConnections();
    void setupTrayIcon();
    void loadSettings();
    void saveSettings();
    void initializeIndex();
    void performSearch();
    void refreshFileList();
    void buildMatchPanel(const Document& doc);
    void updateIndexStatus();
    void applyTheme();

    SearchBar* m_searchBar = nullptr;
    FilePanel* m_filePanel = nullptr;
    FilterPanel* m_filterPanel = nullptr;
    PreviewWidget* m_previewWidget = nullptr;
    MatchPanel* m_matchPanel = nullptr;
    QSplitter* m_hSplitter = nullptr;
    QSplitter* m_vSplitter = nullptr;
    QWidget* m_leftPanel = nullptr;
    QWidget* m_rightPanel = nullptr;
    QToolBar* m_toolbar = nullptr;
    ImportDialog* m_importDialog = nullptr;
    ExportDialog* m_exportDialog = nullptr;

    std::shared_ptr<IndexConfig> m_config;
    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<XapianSearcher> m_searcher;
    std::shared_ptr<ParserManager> m_processorManager;
    std::shared_ptr<FileWatcher> m_fileWatcher;
    std::shared_ptr<IndexQueue> m_indexQueue;

    QString m_currentQuery;
    QString m_currentMatchType = "and";
    QMap<QString, QString> m_currentFilters;
    QString m_currentSortBy;
    bool m_currentSortReverse = false;
    int m_currentPage = 1;
    int m_pageSize = 50;
    int m_themeMode = 0; // 0=绯荤粺榛樿, 1=鏆楄壊, 2=VS Code浜壊
    bool m_sidebarVisible = true;
    QStringList m_searchHistory;
    QSet<QString> m_excludedPaths;
    qint64 m_searchStartTime = 0;
    QMutex m_searchMutex;
    QFutureWatcher<void>* m_searchWatcher = nullptr;
    QMap<QString, qint64> m_lastIndexedTimes;

    QLabel* m_indexStatusLabel = nullptr;
    QLabel* m_searchStatusLabel = nullptr;
    QPushButton* m_cancelSearchBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QSystemTrayIcon* m_trayIcon = nullptr;
    NotificationManager* m_notificationManager = nullptr;
    qint64 m_lastIndexTime = 0;
    qint64 m_lastIndexStartTime = 0;
};

#endif
