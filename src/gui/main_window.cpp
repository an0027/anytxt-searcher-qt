// Must include xapian before Qt to avoid keyword clashes
#include <xapian.h>
#include "gui/main_window.h"
#include "gui/search_bar.h"
#include "gui/results_widget.h"
#include "gui/preview_widget.h"
#include "gui/file_panel.h"
#include "gui/match_panel.h"
#include "dialogs/import_dialog.h"
#include "dialogs/export_dialog.h"
#include "dialogs/about_dialog.h"
#include "dialogs/watch_settings_dialog.h"
#include "core/config.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "core/xapian_searcher.h"
#include "core/document.h"
#include "core/exceptions.h"
#include "parser/parser_manager.h"
#include "core/file_watcher.h"
#include "core/index_queue.h"

#include <QAction>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QMessageBox>
#include <QApplication>
#include <QClipboard>
#include <QFileDialog>
#include <QDebug>
#include <QDateTime>
#include <QtConcurrent>
#include <QCloseEvent>
#include <QRegularExpression>
#include <QSet>

MainWindow::MainWindow(const QString& indexPath, QWidget* parent)
    : QMainWindow(parent)
    , m_searchWatcher(new QFutureWatcher<void>(this))
{
    setWindowTitle(tr("AnyTXT Searcher - 全文搜索工具"));
    resize(1200, 800);
    setMinimumSize(800, 600);

    // Initialize core components
    m_config = std::make_shared<IndexConfig>();
    m_config->load();
    if (!indexPath.isEmpty()) {
        m_config->dbPath = indexPath;
    }
    m_database = std::make_shared<XapianDatabase>();
    m_indexer = std::make_shared<XapianIndexer>(m_database);
    m_searcher = std::make_shared<XapianSearcher>(m_database);
    m_processorManager = std::make_shared<ParserManager>();
    m_fileWatcher = std::make_shared<FileWatcher>(this);
    m_indexQueue = std::make_shared<IndexQueue>(m_database, m_indexer, m_processorManager, this);

    // Setup UI
    setupMenuBar();
    setupToolBar();
    setupCentralWidget();
    setupStatusBar();
    setupConnections();

    // Load settings and initialize index
    loadSettings();
    initializeIndex();

    statusBar()->showMessage(tr("就绪"), 3000);
    qDebug() << "MainWindow initialized, db:" << m_config->dbPath;

    // Load files into file panel
    refreshFileList();
}

MainWindow::~MainWindow()
{
    if (m_indexer) m_indexer->flush();
    saveSettings();
    if (m_searchWatcher->isRunning()) {
        m_searchWatcher->cancel();
        m_searchWatcher->waitForFinished();
    }
}

// ==================== Menu ====================
void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("文件(&F)"));
    QAction* newIndexAction = fileMenu->addAction(tr("新建索引(&N)"));
    newIndexAction->setShortcut(QKeySequence::New);
    QAction* importAction = fileMenu->addAction(tr("导入文档(&I)..."));
    importAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_I));
    QAction* exportAction = fileMenu->addAction(tr("导出结果(&E)..."));
    exportAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("退出(&Q)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* toolsMenu = menuBar()->addMenu(tr("工具(&T)"));
    QAction* watchAction = toolsMenu->addAction(tr("智能索引设置(&W)..."));
    connect(watchAction, &QAction::triggered, this, &MainWindow::onWatchSettings);
    toolsMenu->addSeparator();
    QAction* reindexAction = toolsMenu->addAction(tr("重建索引(&R)..."));
    connect(reindexAction, &QAction::triggered, this, &MainWindow::onReindex);
    QAction* optimizeAction = toolsMenu->addAction(tr("优化索引(&O)"));
    connect(optimizeAction, &QAction::triggered, this, &MainWindow::onOptimize);

    QMenu* viewMenu = menuBar()->addMenu(tr("视图(&V)"));
    QAction* themeAction = viewMenu->addAction(tr("切换主题(&T)"));
    connect(themeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    QAction* aboutAction = helpMenu->addAction(tr("关于(&A)"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onAbout);

    connect(newIndexAction, &QAction::triggered, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, tr("选择索引目录"), m_config->dbPath);
        if (!path.isEmpty()) {
            m_config->dbPath = path;
            m_config->save();
            initializeIndex();
            refreshFileList();
        }
    });
    connect(importAction, &QAction::triggered, this, &MainWindow::onImportRequested);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportRequested);
}

void MainWindow::setupToolBar()
{
    m_toolbar = addToolBar(tr("工具栏"));
    m_toolbar->setMovable(false);
    QAction* refreshAction = m_toolbar->addAction(tr("刷新"));
    connect(refreshAction, &QAction::triggered, this, [this]() {
        if (!m_currentQuery.isEmpty()) performSearch();
    });
}

void MainWindow::setupCentralWidget()
{
    auto* centralWidget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_searchBar = new SearchBar(this);
    mainLayout->addWidget(m_searchBar);

    m_hSplitter = new QSplitter(Qt::Horizontal, this);
    m_hSplitter->setHandleWidth(1);

    m_filePanel = new FilePanel(this);
    m_filePanel->setMinimumWidth(200);
    m_hSplitter->addWidget(m_filePanel);

    m_rightPanel = new QWidget(this);
    auto* rightLayout = new QVBoxLayout(m_rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    m_vSplitter = new QSplitter(Qt::Vertical, this);
    m_vSplitter->setHandleWidth(1);

    m_previewWidget = new PreviewWidget(this);
    m_vSplitter->addWidget(m_previewWidget);

    m_matchPanel = new MatchPanel(this);
    m_vSplitter->addWidget(m_matchPanel);

    m_vSplitter->setStretchFactor(0, 3);
    m_vSplitter->setStretchFactor(1, 2);

    rightLayout->addWidget(m_vSplitter, 1);

    m_hSplitter->addWidget(m_rightPanel);
    m_hSplitter->setStretchFactor(0, 0);
    m_hSplitter->setStretchFactor(1, 1);
    m_hSplitter->setSizes({250, 800});

    mainLayout->addWidget(m_hSplitter, 1);
    setCentralWidget(centralWidget);
}

void MainWindow::setupStatusBar()
{
    m_indexStatusLabel = new QLabel(tr("索引状态: 未初始化"), this);
    m_searchStatusLabel = new QLabel(this);
    m_progressBar = new QProgressBar(this);
    m_progressBar->setMaximumWidth(150);
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setVisible(false);
    statusBar()->addPermanentWidget(m_indexStatusLabel);
    statusBar()->addPermanentWidget(m_searchStatusLabel);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::setupConnections()
{
    connect(m_searchBar, &SearchBar::search, this, &MainWindow::onSearch);
    connect(m_filePanel, &FilePanel::fileSelected, this, &MainWindow::onResultSelected);
    connect(m_previewWidget, &PreviewWidget::openFile, this, &MainWindow::onOpenFile);
    connect(m_previewWidget, &PreviewWidget::copyPath, this, &MainWindow::onCopyPath);
    connect(m_matchPanel, &MatchPanel::paragraphClicked, this, [this](int paraIdx) {
        m_previewWidget->scrollToParagraph(paraIdx);
    });
    connect(m_searchWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onSearchFinished);
    connect(m_fileWatcher.get(), &FileWatcher::filesChanged, this, &MainWindow::onFilesChanged);
    connect(m_indexQueue.get(), &IndexQueue::progressUpdated, this, &MainWindow::onQueueProgress);
    connect(m_indexQueue.get(), &IndexQueue::queueFinished, this, &MainWindow::onQueueFinished);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    m_darkTheme = settings.value("app/darkTheme", false).toBool();
    m_sidebarVisible = settings.value("app/sidebarVisible", true).toBool();
    m_currentPage = settings.value("search/page", 1).toInt();
    m_pageSize = settings.value("search/pageSize", 50).toInt();
    QByteArray geometry = settings.value("app/geometry").toByteArray();
    if (!geometry.isEmpty()) restoreGeometry(geometry);
    QByteArray state = settings.value("app/windowState").toByteArray();
    if (!state.isEmpty()) restoreState(state);
    QString lastQuery = settings.value("search/lastQuery").toString();
    QString lastScope = settings.value("search/lastScope", "all").toString();
    if (!lastQuery.isEmpty()) {
        m_searchBar->setQuery(lastQuery);
        m_searchBar->setScopeCombo(lastScope);
    }
    if (m_darkTheme) onToggleTheme();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("app/darkTheme", m_darkTheme);
    settings.setValue("app/sidebarVisible", m_sidebarVisible);
    settings.setValue("search/page", m_currentPage);
    settings.setValue("search/pageSize", m_pageSize);
    settings.setValue("app/geometry", saveGeometry());
    settings.setValue("app/windowState", saveState());
    settings.setValue("index/path", m_config->dbPath);
}

void MainWindow::initializeIndex()
{
    try {
        m_config->ensureDatabaseDirectory();
        bool newDb = !QDir(m_config->dbPath).exists("postlist.baseB") &&
                     !QDir(m_config->dbPath).exists("postlist.baseA");
        if (newDb) {
            m_database->create(m_config->dbPath);
        } else {
            m_database->open(m_config->dbPath, true);
        }
        m_lastIndexTime = QDateTime::currentSecsSinceEpoch();
        updateIndexStatus();
        m_indexer->setStemLanguage(m_config->stemLanguage);
        m_indexer->setEnableSpelling(m_config->enableSpelling);
        m_indexer->setBatchSize(m_config->batchSize);
        statusBar()->showMessage(tr("索引已就绪"), 3000);
    } catch (const DatabaseError& e) {
        qWarning() << "Index init failed:" << e.what();
        m_indexStatusLabel->setText(tr("索引错误"));
    } catch (const std::exception& e) {
        qWarning() << "Index error:" << e.what();
    }
}

void MainWindow::updateIndexStatus()
{
    int docCount = 0;
    try {
        if (m_database && m_database->isOpen()) docCount = m_database->getDocumentCount();
    } catch (...) {}
    QString status = tr("索引: %1 文档").arg(docCount);
    if (m_lastIndexTime > 0) {
        status += tr(" | 更新: %1").arg(QDateTime::fromSecsSinceEpoch(m_lastIndexTime).toString("HH:mm:ss"));
    }
    m_indexStatusLabel->setText(status);
}

void MainWindow::onSearch(const QString& query, const QVariantMap& options)
{
    QString scope = options.value("scope", "all").toString();
    if (scope == "file") m_currentQuery = "file:" + query;
    else if (scope == "title") m_currentQuery = "title:" + query;
    else m_currentQuery = query;
    m_currentPage = 1;
    m_currentMatchType = options.value("matchType", "and").toString();
    performSearch();
    QSettings settings;
    settings.setValue("search/lastQuery", query);
    settings.setValue("search/lastScope", scope);
}

void MainWindow::performSearch()
{
    if (m_currentQuery.trimmed().isEmpty()) return;
    m_progressBar->setVisible(true);
    m_searchStatusLabel->setText(tr("正在搜索..."));

    QString query = m_currentQuery;
    QMap<QString, QString> filters = m_currentFilters;
    int offset = (m_currentPage - 1) * m_pageSize;
    int limit = m_pageSize;
    QString sortBy = m_currentSortBy;
    bool sortReverse = m_currentSortReverse;
    QString matchType = m_currentMatchType;

    QFuture<void> future = QtConcurrent::run([this, query, filters, offset, limit, sortBy, sortReverse, matchType]() {
        try {
            QPair<QVector<Document>, int> result;
            {
                QMutexLocker locker(&m_searchMutex);
                if (m_indexer) m_indexer->flush();
                result = m_searcher->search(query, offset, limit, filters, sortBy, sortReverse, matchType);
            }
            // Secondary filter: remove CJK bigram false positives
            if (!query.trimmed().isEmpty() && !result.first.isEmpty()) {
                QStringList words = query.toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (!words.isEmpty()) {
                    QVector<Document> verified;
                    for (const auto& doc : result.first) {
                        QString lowerContent = doc.content.toLower();
                        bool hasKeyword = false;
                        for (const auto& kw : words) {
                            if (lowerContent.contains(kw)) { hasKeyword = true; break; }
                        }
                        if (hasKeyword) verified.append(doc);
                    }
                    result.first = verified;
                    result.second = qMin(result.second, verified.size());
                }
            }
            QMetaObject::invokeMethod(this, [this, result]() {
                // Reload file list from index, then filter to show only matches
                try {
                    QVector<Document> allDocs = m_searcher->getAllDocuments();
                    m_filePanel->setFiles(allDocs);
                } catch (...) {}
                QSet<int64_t> matchIds;
                for (const auto& doc : result.first) matchIds.insert(doc.docId);
                m_filePanel->setMatchIds(matchIds);
                m_searchStatusLabel->setText(tr("找到 %1 个结果").arg(result.second));
                m_progressBar->setVisible(false);
            }, Qt::QueuedConnection);
        } catch (const InvalidQueryError& e) {
            QMetaObject::invokeMethod(this, [this, e]() {
                QMessageBox::warning(const_cast<MainWindow*>(this), tr("查询错误"), tr("%1").arg(e.what()));
                m_searchStatusLabel->setText(tr("查询错误"));
                m_progressBar->setVisible(false);
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, e]() {
                m_searchStatusLabel->setText(tr("搜索失败"));
                m_progressBar->setVisible(false);
                qWarning() << "Search error:" << e.what();
            }, Qt::QueuedConnection);
        }
    });
    m_searchWatcher->setFuture(future);
}

void MainWindow::onSearchFinished()
{
    m_searchWatcher->setFuture(QFuture<void>());
}

void MainWindow::refreshFileList()
{
    try {
        QVector<Document> allDocs = m_searcher->getAllDocuments();
        m_filePanel->setFiles(allDocs);
        qDebug() << "Loaded" << allDocs.size() << "documents";
    } catch (const std::exception& e) {
        qWarning() << "refreshFileList failed:" << e.what();
    }
}

void MainWindow::buildMatchPanel(const Document& doc)
{
    if (m_currentQuery.trimmed().isEmpty() || doc.content.isEmpty()) {
        m_matchPanel->clear();
        return;
    }
    QStringList keywords = m_currentQuery.toLower().split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (keywords.isEmpty()) { m_matchPanel->clear(); return; }
    QStringList lines = doc.content.split('\n');
    QVector<MatchItem> items;
    for (int i = 0; i < lines.size(); ++i) {
        QString lowerLine = lines[i].toLower();
        for (const auto& kw : keywords) {
            if (lowerLine.contains(kw)) {
                MatchItem mi;
                mi.paragraphIndex = i;
                mi.snippet = lines[i].trimmed().left(300);
                mi.matchPos = lowerLine.indexOf(kw);
                items.append(mi);
                break;
            }
        }
    }
    m_matchPanel->setMatches(items, items.size());
}

void MainWindow::onResultSelected(const Document& doc)
{
    if (doc.docId >= 0) {
        m_previewWidget->setDocument(doc, m_currentQuery);
        buildMatchPanel(doc);
    }
}

void MainWindow::onOpenFile(const Document& doc)
{
    QFileInfo fi(doc.filePath);
    if (fi.exists()) QDesktopServices::openUrl(QUrl::fromLocalFile(doc.filePath));
    else QMessageBox::warning(this, tr("文件不存在"), tr("'%1' 不存在").arg(doc.filePath));
}

void MainWindow::onCopyPath(const QString& path)
{
    QApplication::clipboard()->setText(path);
    statusBar()->showMessage(tr("路径已复制"), 2000);
}

void MainWindow::onImportRequested()
{
    if (!m_importDialog) {
        m_importDialog = new ImportDialog(m_database, m_indexer, m_processorManager, this);
    }
    m_importDialog->exec();
    int imported = m_importDialog->importedCount();
    if (imported > 0) {
        updateIndexStatus();
        refreshFileList();
        if (!m_currentQuery.isEmpty()) performSearch();
    }
}

void MainWindow::onExportRequested()
{
    QMessageBox::information(this, tr("导出"), tr("导出功能开发中"));
}

void MainWindow::onReindex()
{
    if (QMessageBox::question(this, tr("重建索引"), tr("确定要重建吗？")) == QMessageBox::Yes) {
        try {
            m_indexer->clearIndex();
            refreshFileList();
            updateIndexStatus();
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("错误"), tr("%1").arg(e.what()));
        }
    }
}

void MainWindow::onOptimize()
{
    try {
        m_indexer->optimize();
        statusBar()->showMessage(tr("索引优化完成"), 3000);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("错误"), tr("%1").arg(e.what()));
    }
}

void MainWindow::onToggleTheme()
{
    m_darkTheme = !m_darkTheme;
    if (m_darkTheme) {
        setStyleSheet(
            "QMainWindow { background-color: #1e1e1e; color: #d4d4d4; }"
            "QWidget { background-color: #1e1e1e; color: #d4d4d4; }"
            "QMenuBar { background-color: #2d2d2d; color: #d4d4d4; }"
            "QToolBar { background-color: #2d2d2d; border: none; }"
            "QStatusBar { background-color: #007acc; color: white; }");
    } else {
        setStyleSheet("");
    }
}

void MainWindow::onToggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
}

void MainWindow::onAddWatchFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择监听文件夹"));
    if (!dir.isEmpty() && !m_config->watchedFolders.contains(dir)) {
        m_config->watchedFolders.append(dir);
        m_config->save();
        if (m_config->enableFileWatching) {
            m_fileWatcher->addWatchPath(dir);
            m_fileWatcher->start();
            if (!m_indexQueue->isProcessing()) m_indexQueue->start();
        }
    }
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onPreferences()
{
    QMessageBox::information(this, tr("偏好设置"), tr("偏好设置功能开发中"));
}

void MainWindow::onSortChanged(const QString& sortBy, bool reverse)
{
    m_currentSortBy = sortBy;
    m_currentSortReverse = reverse;
    if (!m_currentQuery.isEmpty()) performSearch();
}

void MainWindow::onWatchSettings()
{
    QStringList folders = m_config->watchedFolders;
    WatchSettingsDialog dialog(folders, m_config->enableFileWatching, m_config->watchIntervalMs, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_config->watchedFolders = dialog.watchedFolders();
        m_config->enableFileWatching = dialog.isWatchingEnabled();
        m_config->watchIntervalMs = dialog.watchInterval();
        m_config->save();
        m_fileWatcher->stop();
        for (const auto& path : folders) {
            if (!m_config->watchedFolders.contains(path)) m_fileWatcher->removeWatchPath(path);
        }
        if (m_config->enableFileWatching && !m_config->watchedFolders.isEmpty()) {
            m_fileWatcher->setWatchInterval(m_config->watchIntervalMs);
            for (const auto& path : m_config->watchedFolders) m_fileWatcher->addWatchPath(path);
            m_fileWatcher->start();
            if (!m_indexQueue->isProcessing()) m_indexQueue->start();
        }
    }
}

void MainWindow::onFilesChanged(const QStringList& newFiles, const QStringList& modifiedFiles, const QStringList& deletedFiles)
{
    Q_UNUSED(modifiedFiles);
    qint64 now = QDateTime::currentSecsSinceEpoch();
    QStringList deduped;
    for (const auto& path : newFiles) {
        auto it = m_lastIndexedTimes.find(path);
        if (it == m_lastIndexedTimes.end() || now - it.value() > 5) {
            m_lastIndexedTimes[path] = now;
            deduped.append(path);
        }
    }
    if (!deduped.isEmpty()) m_indexQueue->enqueueBatch(deduped);
    for (const auto& path : deletedFiles) {
        try { m_indexer->deleteDocument(path); }
        catch (const std::exception& e) { qWarning() << "Delete failed:" << e.what(); }
    }
    if (!m_indexQueue->isProcessing() && !deduped.isEmpty()) m_indexQueue->start();
}

void MainWindow::onQueueProgress(int indexed, int total, const QString& currentFile)
{
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(indexed);
}

void MainWindow::onQueueFinished(int indexed, int failed)
{
    m_progressBar->setVisible(false);
    if (indexed > 0 || failed > 0) {
        m_lastIndexTime = QDateTime::currentSecsSinceEpoch();
        updateIndexStatus();
    }
    if (m_database && m_database->isOpen()) m_database->refresh();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSettings();
    QMainWindow::closeEvent(event);
}
