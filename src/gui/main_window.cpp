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
#include "dialogs/help_dialog.h"
#include "dialogs/preferences_dialog.h"
#include "core/notification_manager.h"
#include "dialogs/watch_settings_dialog.h"
#include "core/config.h"
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "core/xapian_searcher.h"
#include "core/document.h"
#include "core/exceptions.h"
#include "parser/parser_manager.h"
#include "core/file_watcher.h"
#include "utils/file_utils.h"
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
#include <QMenu>
#include <QIcon>

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
    setupTrayIcon();

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

    // 编辑菜单
    QMenu* editMenu = menuBar()->addMenu(tr("编辑(&E)"));
    QAction* manageExcludeAction = editMenu->addAction(tr("管理排除项(&X)..."));
    connect(manageExcludeAction, &QAction::triggered, this, [this]() {
        if (m_excludedPaths.isEmpty()) {
            QMessageBox::information(this, tr("排除项"), tr("当前没有排除的文件。"));
            return;
        }
        QString msg = tr("已排除以下文件:\n\n");
        QStringList items = m_excludedPaths.values();
        for (int i = 0; i < qMin(items.size(), 20); ++i)
            msg += QStringLiteral("  • %1\n").arg(items[i]);
        if (items.size() > 20)
            msg += tr("  ... 还有 %1 个\n\n").arg(items.size() - 20);
        msg += tr("\n是否清除所有排除项？");
        if (QMessageBox::question(this, tr("管理排除项"), msg) == QMessageBox::Yes) {
            m_excludedPaths.clear();
            saveSettings();
            if (!m_currentQuery.isEmpty()) performSearch();
            statusBar()->showMessage(tr("已清除所有排除项"), 3000);
        }
    });

    QMenu* toolsMenu = menuBar()->addMenu(tr("工具(&T)"));
    QAction* watchAction = toolsMenu->addAction(tr("智能索引设置(&W)..."));
    connect(watchAction, &QAction::triggered, this, &MainWindow::onWatchSettings);
    toolsMenu->addSeparator();
    QAction* reindexAction = toolsMenu->addAction(tr("重建索引(&R)..."));
    connect(reindexAction, &QAction::triggered, this, &MainWindow::onReindex);
    QAction* optimizeAction = toolsMenu->addAction(tr("优化索引(&O)"));
    connect(optimizeAction, &QAction::triggered, this, &MainWindow::onOptimize);

    QMenu* viewMenu = menuBar()->addMenu(tr("视图(&V)"));

    QAction* sidebarAction = viewMenu->addAction(tr("侧边栏(&S)"));
    sidebarAction->setCheckable(true);
    sidebarAction->setChecked(m_sidebarVisible);
    sidebarAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_ParenLeft));
    connect(sidebarAction, &QAction::toggled, this, &MainWindow::onToggleSidebar);

    QAction* statusBarAction = viewMenu->addAction(tr("状态栏(&B)"));
    statusBarAction->setCheckable(true);
    statusBarAction->setChecked(true);
    connect(statusBarAction, &QAction::toggled, this, [this](bool visible) {
        statusBar()->setVisible(visible);
    });

    viewMenu->addSeparator();

    QAction* fullscreenAction = viewMenu->addAction(tr("全屏(&F)"));
    fullscreenAction->setShortcut(QKeySequence(Qt::Key_F11));
    connect(fullscreenAction, &QAction::triggered, this, [this]() {
        if (isFullScreen())
            showNormal();
        else
            showFullScreen();
    });

    QAction* resetLayoutAction = viewMenu->addAction(tr("重置布局(&R)"));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        m_hSplitter->setSizes({250, 800});
        statusBar()->showMessage(tr("布局已重置"), 3000);
    });

    viewMenu->addSeparator();

    QAction* themeAction = viewMenu->addAction(tr("切换主题(&T)"));
    themeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_T));
    connect(themeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);

    QAction* focusSearchAction = viewMenu->addAction(tr("聚焦搜索框(&C)"));
    focusSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(focusSearchAction, &QAction::triggered, this, [this]() { m_searchBar->focusSearch(); });

    QMenu* helpMenu = menuBar()->addMenu(tr("帮助(&H)"));
    QAction* helpAction = helpMenu->addAction(tr("使用手册(&U)"));
    helpAction->setShortcut(QKeySequence(Qt::Key_F1));
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(tr("关于(&A)"));
    connect(helpAction, &QAction::triggered, this, &MainWindow::onHelp);
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

    // Left panel: file list
    m_leftPanel = new QWidget(this);
    auto* leftLayout = new QVBoxLayout(m_leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(0);

    m_filePanel = new FilePanel(this);
    leftLayout->addWidget(m_filePanel, 1);

    m_hSplitter->addWidget(m_leftPanel);

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

    m_cancelSearchBtn = new QPushButton(tr("取消"), this);
    m_cancelSearchBtn->setFixedSize(50, 22);
    m_cancelSearchBtn->setVisible(false);
    m_cancelSearchBtn->setStyleSheet(
        "QPushButton { background-color: #d32f2f; color: white; border: none; "
        "border-radius: 3px; font-size: 11px; font-weight: bold; }"
        "QPushButton:hover { background-color: #b71c1c; }");
    connect(m_cancelSearchBtn, &QPushButton::clicked, this, [this]() {
        if (m_searchWatcher && m_searchWatcher->isRunning()) {
            m_searchWatcher->cancel();
            m_searchWatcher->waitForFinished();
            m_searchStatusLabel->setText(tr("搜索已取消"));
            m_cancelSearchBtn->setVisible(false);
            m_progressBar->setVisible(false);
        }
    });

    statusBar()->addPermanentWidget(m_indexStatusLabel);
    statusBar()->addPermanentWidget(m_searchStatusLabel);
    statusBar()->addWidget(m_cancelSearchBtn);
    statusBar()->addPermanentWidget(m_progressBar);
}

void MainWindow::setupConnections()
{
    connect(m_searchBar, &SearchBar::search, this, &MainWindow::onSearch);
    connect(m_filePanel, &FilePanel::excludePath, this, [this](const QString& path) {
        m_excludedPaths.insert(path);
        saveSettings();
        if (!m_currentQuery.isEmpty()) performSearch();
        statusBar()->showMessage(tr("已排除: %1").arg(QFileInfo(path).fileName()), 3000);
    });

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
    m_themeMode = settings.value("app/themeMode", 0).toInt();
    m_sidebarVisible = settings.value("app/sidebarVisible", true).toBool();
    m_currentPage = settings.value("search/page", 1).toInt();
    m_pageSize = settings.value("search/pageSize", 50).toInt();
    QStringList excludedPaths = settings.value("app/excludedPaths", QStringList()).toStringList();
    m_excludedPaths = QSet<QString>(excludedPaths.begin(), excludedPaths.end());
    m_searchHistory = settings.value("search/history", QStringList()).toStringList();
    m_searchBar->setHistory(m_searchHistory);
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
    if (!m_sidebarVisible) m_leftPanel->setVisible(false);
    if (m_themeMode != 0) applyTheme();
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue("app/themeMode", m_themeMode);
    settings.setValue("app/sidebarVisible", m_sidebarVisible);
    settings.setValue("search/page", m_currentPage);
    settings.setValue("search/pageSize", m_pageSize);
    settings.setValue("app/geometry", saveGeometry());
    settings.setValue("app/windowState", saveState());
    settings.setValue("index/path", m_config->dbPath);
    settings.setValue("app/excludedPaths", QStringList(m_excludedPaths.begin(), m_excludedPaths.end()));
    settings.setValue("search/history", m_searchHistory);
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

    // Add to search history
    if (!query.isEmpty()) {
        m_searchHistory.removeAll(query);
        m_searchHistory.prepend(query);
        if (m_searchHistory.size() > 15)
            m_searchHistory = m_searchHistory.mid(0, 15);
        m_searchBar->setHistory(m_searchHistory);
    }

    performSearch();
    QSettings settings;
    settings.setValue("search/lastQuery", query);
    settings.setValue("search/lastScope", scope);
}

void MainWindow::performSearch()
{
    if (m_currentQuery.trimmed().isEmpty()) return;
    m_progressBar->setVisible(true);
    m_cancelSearchBtn->setVisible(true);
    m_searchStartTime = QDateTime::currentMSecsSinceEpoch();
    m_searchStatusLabel->setText(tr("正在搜索..."));

    QString query = m_currentQuery;
    QSet<QString> excluded = m_excludedPaths;
    QMap<QString, QString> filters = m_currentFilters;
    int offset = (m_currentPage - 1) * m_pageSize;
    int limit = m_pageSize;
    QString sortBy = m_currentSortBy;
    bool sortReverse = m_currentSortReverse;
    QString matchType = m_currentMatchType;

    QFuture<void> future = QtConcurrent::run([this, query, excluded, filters, offset, limit, sortBy, sortReverse, matchType]() {
        try {
            QPair<QVector<Document>, int> result;
            {
                QMutexLocker locker(&m_searchMutex);
                if (m_indexer) m_indexer->flush();
                result = m_searcher->search(query, offset, limit, filters, sortBy, sortReverse, matchType);
            }
            // Filter excluded paths
            if (!excluded.isEmpty() && !result.first.isEmpty()) {
                result.first.erase(
                    std::remove_if(result.first.begin(), result.first.end(),
                        [&](const Document& d) { return excluded.contains(d.filePath); }),
                    result.first.end());
                result.second = qMin(result.second, result.first.size());
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
                qint64 elapsed = m_searchStartTime > 0
                    ? QDateTime::currentMSecsSinceEpoch() - m_searchStartTime
                    : 0;
                m_searchStatusLabel->setText(
                    tr("找到 %1 个结果 (用时 %2 毫秒)").arg(result.second).arg(elapsed));
                m_progressBar->setVisible(false);
                m_cancelSearchBtn->setVisible(false);
                // Tray notification
                if (m_notificationManager && result.second > 0) {
                    m_notificationManager->notifySearchComplete(result.second);
                }
            }, Qt::QueuedConnection);
        } catch (const InvalidQueryError& e) {
            QMetaObject::invokeMethod(this, [this, e]() {
                QMessageBox::warning(const_cast<MainWindow*>(this), tr("查询错误"), tr("%1").arg(e.what()));
                m_searchStatusLabel->setText(tr("查询错误"));
                m_progressBar->setVisible(false);
                m_cancelSearchBtn->setVisible(false);
            }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(this, [this, e]() {
                m_searchStatusLabel->setText(tr("搜索失败"));
                m_progressBar->setVisible(false);
                m_cancelSearchBtn->setVisible(false);
                qWarning() << "Search error:" << e.what();
            }, Qt::QueuedConnection);
        }
    });
    m_searchWatcher->setFuture(future);
}

void MainWindow::onSearchFinished()
{
    m_cancelSearchBtn->setVisible(false);
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
    // Collect all results for export
    try {
        QVector<Document> allDocs = m_searcher->getAllDocuments();
        if (allDocs.isEmpty()) {
            QMessageBox::information(this, tr("导出"), tr("没有可导出的数据。"));
            return;
        }

        QString filePath = QFileDialog::getSaveFileName(this, tr("导出结果"),
            QString(), tr("CSV 文件 (*.csv);;文本文件 (*.txt)"));
        if (filePath.isEmpty()) return;

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::critical(this, tr("导出失败"), tr("无法写入文件: %1").arg(file.errorString()));
            return;
        }

        QTextStream out(&file);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        out.setCodec("UTF-8");
#endif

        if (filePath.endsWith(".csv", Qt::CaseInsensitive)) {
            out << QStringLiteral("名称,路径,大小,修改时间,类型,相关性\r\n");
            for (const auto& doc : allDocs) {
                if (m_excludedPaths.contains(doc.filePath)) continue;
                QString name = doc.fileName;
                name.replace(QStringLiteral("\""), QStringLiteral("\"\""));
                QString path = doc.filePath;
                path.replace(QStringLiteral("\""), QStringLiteral("\"\""));
                out << QStringLiteral("\"%1\",\"%2\",%3,%4,%5,%6\r\n")
                    .arg(name, path)
                    .arg(doc.fileSize)
                    .arg(QDateTime::fromSecsSinceEpoch(doc.modifiedTime).toString("yyyy-MM-dd HH:mm"))
                    .arg(doc.fileExt.toUpper())
                    .arg(doc.percent);
            }
        } else {
            out << tr("===== AnyTXT Searcher 搜索结果导出 =====\r\n\r\n");
            int count = 0;
            for (const auto& doc : allDocs) {
                if (m_excludedPaths.contains(doc.filePath)) continue;
                count++;
                out << tr("[%1] %2\r\n").arg(count).arg(doc.fileName);
                out << tr("  路径: %1\r\n").arg(doc.filePath);
                out << tr("  大小: %1 | 修改: %2 | 类型: %3 | 相关性: %4%\r\n")
                    .arg(FileUtils::formatFileSize(doc.fileSize))
                    .arg(QDateTime::fromSecsSinceEpoch(doc.modifiedTime).toString("yyyy-MM-dd HH:mm"))
                    .arg(doc.fileExt.toUpper())
                    .arg(doc.percent);
                if (!doc.content.isEmpty()) {
                    out << tr("  预览: %1...\r\n\r\n")
                        .arg(doc.content.left(200).simplified());
                }
            }
        }

        file.close();
        statusBar()->showMessage(tr("导出完成: %1 条记录").arg(allDocs.size()), 5000);
    } catch (const std::exception& e) {
        QMessageBox::critical(this, tr("导出失败"), tr("%1").arg(e.what()));
    }
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

void MainWindow::applyTheme()
{
    if (m_themeMode == 1) {
        // ── 暗色主题 ──
        setStyleSheet(
            "QMainWindow, QWidget { background-color: #1e1e1e; color: #d4d4d4; }"
            "QMainWindow::separator { background-color: #3c3c3c; width: 1px; height: 1px; }"

            "QMenuBar { background-color: #2d2d2d; color: #d4d4d4; border-bottom: 1px solid #3c3c3c; }"
            "QMenuBar::item:selected { background-color: #094771; }"
            "QMenu { background-color: #2d2d2d; color: #d4d4d4; border: 1px solid #3c3c3c; }"
            "QMenu::item:selected { background-color: #094771; }"
            "QMenu::separator { height: 1px; background: #3c3c3c; margin: 4px 8px; }"

            "QToolBar { background-color: #2d2d2d; border: none; border-bottom: 1px solid #3c3c3c; spacing: 4px; padding: 2px; }"
            "QToolBar QToolButton { color: #d4d4d4; border: none; padding: 4px 8px; border-radius: 3px; }"
            "QToolBar QToolButton:hover { background-color: #3c3c3c; }"
            "QToolBar QToolButton:pressed { background-color: #094771; }"

            "QStatusBar { background-color: #007acc; color: white; font-size: 12px; }"
            "QStatusBar QLabel { color: white; }"

            "QSplitter::handle { background-color: #3c3c3c; }"

            "QLabel { color: #d4d4d4; background: transparent; }"

            "QComboBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; border-radius: 4px; padding: 4px 8px; }"
            "QComboBox::drop-down { border: none; width: 20px; }"
            "QComboBox QAbstractItemView { background-color: #3c3c3c; color: #d4d4d4; selection-background-color: #094771; }"

            "QLineEdit { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; border-radius: 4px; padding: 4px 8px; }"
            "QLineEdit:focus { border-color: #1976D2; }"

            "QPushButton { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; border-radius: 4px; padding: 6px 16px; }"
            "QPushButton:hover { background-color: #4c4c4c; }"
            "QPushButton:pressed { background-color: #094771; }"
            "QPushButton:disabled { color: #666; background-color: #2d2d2d; }"

            "QTreeWidget { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; alternate-background-color: #252525; }"
            "QTreeWidget::item:selected { background-color: #094771; }"
            "QTreeWidget::item:hover { background-color: #2a2d2e; }"
            "QHeaderView::section { background-color: #2d2d2d; color: #d4d4d4; border: 1px solid #3c3c3c; padding: 4px; }"

            "QListWidget { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; }"
            "QListWidget::item:selected { background-color: #094771; }"
            "QListWidget::item:hover { background-color: #2a2d2e; }"

            "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3c3c3c; }"
            "QTextEdit QScrollBar:vertical { background: #2d2d2d; width: 10px; }"
            "QTextEdit QScrollBar::handle:vertical { background: #555; border-radius: 4px; min-height: 20px; }"
            "QTextEdit QScrollBar::add-line:vertical, QTextEdit QScrollBar::sub-line:vertical { height: 0; }"
            "QTextEdit QScrollBar:horizontal { background: #2d2d2d; height: 10px; }"
            "QTextEdit QScrollBar::handle:horizontal { background: #555; border-radius: 4px; min-width: 20px; }"
            "QTextEdit QScrollBar::add-line:horizontal, QTextEdit QScrollBar::sub-line:horizontal { width: 0; }"

            "QTabWidget::pane { background-color: #1e1e1e; border: 1px solid #3c3c3c; }"
            "QTabBar::tab { background-color: #2d2d2d; color: #d4d4d4; padding: 6px 16px; border: 1px solid #3c3c3c; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
            "QTabBar::tab:selected { background-color: #1e1e1e; border-bottom: 1px solid #1e1e1e; }"
            "QTabBar::tab:hover { background-color: #3c3c3c; }"

            "QProgressBar { background-color: #2d2d2d; border: 1px solid #555; border-radius: 3px; text-align: center; color: #d4d4d4; }"
            "QProgressBar::chunk { background-color: #1976D2; border-radius: 2px; }"

            "QCheckBox { color: #d4d4d4; spacing: 4px; }"
            "QCheckBox::indicator { width: 14px; height: 14px; }"
            "QRadioButton { color: #d4d4d4; spacing: 4px; }"

            "QGroupBox { color: #d4d4d4; border: 1px solid #3c3c3c; border-radius: 4px; margin-top: 8px; padding-top: 12px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"

            "QSpinBox { background-color: #3c3c3c; color: #d4d4d4; border: 1px solid #555; border-radius: 4px; padding: 2px 4px; }"
        );
    } else if (m_themeMode == 2) {
        // ── VS Code 风格亮色主题 ──
        setStyleSheet(
            "QMainWindow, QWidget { background-color: #ffffff; color: #1f1f1f; }"
            "QMainWindow::separator { background-color: #e0e0e0; width: 1px; height: 1px; }"

            "QMenuBar { background-color: #f3f3f3; color: #1f1f1f; border-bottom: 1px solid #e0e0e0; }"
            "QMenuBar::item:selected { background-color: #e8f0fe; color: #0066b8; }"
            "QMenu { background-color: #ffffff; color: #1f1f1f; border: 1px solid #d0d0d0; }"
            "QMenu::item:selected { background-color: #e8f0fe; color: #0066b8; }"
            "QMenu::separator { height: 1px; background: #e0e0e0; margin: 4px 8px; }"

            "QToolBar { background-color: #f3f3f3; border: none; border-bottom: 1px solid #e0e0e0; spacing: 4px; padding: 2px; }"
            "QToolBar QToolButton { color: #1f1f1f; border: none; padding: 4px 8px; border-radius: 3px; }"
            "QToolBar QToolButton:hover { background-color: #e0e0e0; }"
            "QToolBar QToolButton:pressed { background-color: #cce5ff; }"

            "QStatusBar { background-color: #0066b8; color: white; font-size: 12px; }"
            "QStatusBar QLabel { color: white; }"

            "QSplitter::handle { background-color: #e0e0e0; }"

            "QLabel { color: #1f1f1f; background: transparent; }"

            "QComboBox { background-color: #ffffff; color: #1f1f1f; border: 1px solid #c0c0c0; border-radius: 4px; padding: 4px 8px; }"
            "QComboBox::drop-down { border: none; width: 20px; }"
            "QComboBox QAbstractItemView { background-color: #ffffff; color: #1f1f1f; selection-background-color: #e8f0fe; selection-color: #0066b8; }"

            "QLineEdit { background-color: #ffffff; color: #1f1f1f; border: 1px solid #c0c0c0; border-radius: 4px; padding: 4px 8px; }"
            "QLineEdit:focus { border-color: #0066b8; }"

            "QPushButton { background-color: #ffffff; color: #1f1f1f; border: 1px solid #c0c0c0; border-radius: 4px; padding: 6px 16px; }"
            "QPushButton:hover { background-color: #e8f0fe; border-color: #0066b8; color: #0066b8; }"
            "QPushButton:pressed { background-color: #cce5ff; }"
            "QPushButton:disabled { color: #aaa; background-color: #f5f5f5; border-color: #e0e0e0; }"

            "QTreeWidget { background-color: #ffffff; color: #1f1f1f; border: 1px solid #e0e0e0; alternate-background-color: #f8f8f8; }"
            "QTreeWidget::item:selected { background-color: #e8f0fe; color: #1f1f1f; }"
            "QTreeWidget::item:hover { background-color: #f0f0f0; }"
            "QHeaderView::section { background-color: #f3f3f3; color: #1f1f1f; border: 1px solid #e0e0e0; padding: 4px; font-weight: 500; }"

            "QListWidget { background-color: #ffffff; color: #1f1f1f; border: 1px solid #e0e0e0; }"
            "QListWidget::item:selected { background-color: #e8f0fe; color: #1f1f1f; }"
            "QListWidget::item:hover { background-color: #f0f0f0; }"

            "QTextEdit { background-color: #ffffff; color: #1f1f1f; border: 1px solid #e0e0e0; }"
            "QTextEdit QScrollBar:vertical { background: #f3f3f3; width: 10px; }"
            "QTextEdit QScrollBar::handle:vertical { background: #c0c0c0; border-radius: 4px; min-height: 20px; }"
            "QTextEdit QScrollBar::add-line:vertical, QTextEdit QScrollBar::sub-line:vertical { height: 0; }"
            "QTextEdit QScrollBar:horizontal { background: #f3f3f3; height: 10px; }"
            "QTextEdit QScrollBar::handle:horizontal { background: #c0c0c0; border-radius: 4px; min-width: 20px; }"
            "QTextEdit QScrollBar::add-line:horizontal, QTextEdit QScrollBar::sub-line:horizontal { width: 0; }"

            "QTabWidget::pane { background-color: #ffffff; border: 1px solid #e0e0e0; }"
            "QTabBar::tab { background-color: #ececec; color: #1f1f1f; padding: 6px 16px; border: 1px solid #e0e0e0; border-bottom: none; border-top-left-radius: 4px; border-top-right-radius: 4px; }"
            "QTabBar::tab:selected { background-color: #ffffff; border-bottom: 1px solid #ffffff; color: #0066b8; }"
            "QTabBar::tab:hover { background-color: #e8f0fe; }"

            "QProgressBar { background-color: #f3f3f3; border: 1px solid #e0e0e0; border-radius: 3px; text-align: center; color: #1f1f1f; }"
            "QProgressBar::chunk { background-color: #0066b8; border-radius: 2px; }"

            "QCheckBox { color: #1f1f1f; spacing: 4px; }"
            "QCheckBox::indicator { width: 14px; height: 14px; }"
            "QRadioButton { color: #1f1f1f; spacing: 4px; }"

            "QGroupBox { color: #1f1f1f; border: 1px solid #e0e0e0; border-radius: 4px; margin-top: 8px; padding-top: 12px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }"

            "QSpinBox { background-color: #ffffff; color: #1f1f1f; border: 1px solid #c0c0c0; border-radius: 4px; padding: 2px 4px; }"
        );
    } else {
        // ── 系统默认（无样式）──
        setStyleSheet("");
    }
}

void MainWindow::onToggleTheme()
{
    // 循环: 系统默认(0) → 暗色(1) → VS Code亮色(2) → 系统默认(0)
    m_themeMode = (m_themeMode + 1) % 3;
    applyTheme();
    QSettings settings;
    settings.setValue("app/themeMode", m_themeMode);
    QStringList names = {tr("系统默认"), tr("暗色主题"), tr("VS Code 亮色")};
    statusBar()->showMessage(tr("主题: %1").arg(names[m_themeMode]), 3000);
}

void MainWindow::onToggleSidebar()
{
    m_sidebarVisible = !m_sidebarVisible;
    m_leftPanel->setVisible(m_sidebarVisible);
    QSettings settings;
    settings.setValue("app/sidebarVisible", m_sidebarVisible);
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

void MainWindow::onHelp()
{
    HelpDialog dialog(this);
    dialog.exec();
}

void MainWindow::onAbout()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::onPreferences()
{
    PreferencesDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Apply display settings
        int newTheme = dialog.themeMode();
        if (newTheme != m_themeMode) {
            m_themeMode = newTheme;
            applyTheme();
        }

        // Apply search settings
        m_pageSize = dialog.pageSize();
        m_searchBar->setScopeCombo(dialog.defaultScope());

        // Apply index settings
        m_config->batchSize = dialog.batchSize();
        m_config->enableSpelling = dialog.enableSpelling();
        if (m_indexer) {
            m_indexer->setBatchSize(m_config->batchSize);
            m_indexer->setEnableSpelling(m_config->enableSpelling);
        }
        m_config->save();

        statusBar()->showMessage(tr("设置已应用"), 3000);
    }
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

    // Track start time on first progress
    if (m_lastIndexStartTime == 0)
        m_lastIndexStartTime = QDateTime::currentSecsSinceEpoch();

    // Track start time on first progress
    if (m_lastIndexStartTime == 0)
        m_lastIndexStartTime = QDateTime::currentSecsSinceEpoch();
}

void MainWindow::onQueueFinished(int indexed, int failed)
{
    m_progressBar->setVisible(false);
    if (indexed > 0 || failed > 0) {
        m_lastIndexTime = QDateTime::currentSecsSinceEpoch();
        updateIndexStatus();
        // Notification
        if (m_notificationManager) {
            qint64 elapsed = m_lastIndexStartTime > 0
                ? QDateTime::currentSecsSinceEpoch() - m_lastIndexStartTime
                : 0;
            m_notificationManager->notifyIndexComplete(indexed, failed, elapsed);
        }
    }
    if (m_database && m_database->isOpen()) m_database->refresh();
}

void MainWindow::setupTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;

    m_trayIcon = new QSystemTrayIcon(this);
    QIcon appIcon = QApplication::windowIcon();
    if (appIcon.isNull()) {
        // Fallback: create a simple pixmap icon
        QPixmap pix(32, 32);
        pix.fill(QColor("#1976D2"));
        appIcon = QIcon(pix);
    }
    m_trayIcon->setIcon(appIcon);
    m_trayIcon->setToolTip(tr("AnyTXT Searcher"));

    // Context menu
    auto* trayMenu = new QMenu(this);
    QAction* showAction = trayMenu->addAction(tr("显示主窗口"));
    QAction* searchAction = trayMenu->addAction(tr("搜索..."));
    trayMenu->addSeparator();
    QAction* exitAction = trayMenu->addAction(tr("退出"));

    connect(showAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
        raise();
    });
    connect(searchAction, &QAction::triggered, this, [this]() {
        showNormal();
        activateWindow();
        raise();
        m_searchBar->focusSearch();
    });
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);

    m_trayIcon->setContextMenu(trayMenu);

    // Double-click restores window
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            showNormal();
            activateWindow();
            raise();
        }
    });

    m_trayIcon->show();

    // Initialize notification manager
    m_notificationManager = new NotificationManager(m_trayIcon, this);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        // Minimize to tray instead of closing
        hide();
        m_trayIcon->showMessage(
            tr("AnyTXT Searcher"),
            tr("程序已最小化到系统托盘，后台任务继续执行"),
            QSystemTrayIcon::Information, 3000);
        event->ignore();
    } else {
        saveSettings();
        QMainWindow::closeEvent(event);
    }
}
