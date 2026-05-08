// Must include xapian before Qt to avoid keyword clashes
#include <xapian.h>
#include "dialogs/import_dialog.h"
#include <QDateTime>
#include <QHeaderView>
#include <QScrollBar>
#include "core/xapian_database.h"
#include "core/xapian_indexer.h"
#include "parser/parser_manager.h"
#include <QFileDialog>
#include <QFileInfo>
#include <QDirIterator>
#include <QMessageBox>
#include <QDebug>

// ========== ImportWorker Implementation ==========

ImportWorker::ImportWorker(std::shared_ptr<XapianDatabase> db,
                           std::shared_ptr<XapianIndexer> indexer,
                           std::shared_ptr<ParserManager> processor,
                           const QStringList& files)
    : m_database(std::move(db))
    , m_indexer(std::move(indexer))
    , m_processor(std::move(processor))
    , m_files(files)
{
}

void ImportWorker::process()
{
    int imported = 0;
    int failed = 0;
    int skipped = 0;
    int total = m_files.size();

    emit logMessage(QString("开始导入 %1 个文件...").arg(total));
    emit progressUpdated(0, total, "");

    for (int i = 0; i < total; ++i) {
        if (m_cancelled) {
            emit logMessage("导入已取消");
            emit finished(imported, failed, skipped);
            return;
        }

        const QString& filePath = m_files[i];
        QFileInfo fi(filePath);

        emit progressUpdated(i + 1, total, fi.fileName());
        emit logMessage(QString("[%1/%2] 处理: %3").arg(i + 1).arg(total).arg(fi.fileName()));

        try {
            // Process the document
            DocumentProcessor::Result result = m_processor->processDocument(filePath);

            if (!result.success) {
                skipped++;
                emit logMessage(QString("  ⚠ 跳过: %1").arg(result.errorMessage));
                continue;
            }

            // Build metadata map
            QMap<QString, QString> metadata = result.metadata;
            metadata["filePath"] = fi.absoluteFilePath();
            metadata["fileName"] = fi.fileName();
            metadata["fileSize"] = QString::number(fi.size());
            metadata["modifiedTime"] = QString::number(fi.lastModified().toSecsSinceEpoch());

            if (!result.text.isEmpty()) {
                // Index it
                m_indexer->addDocument(filePath, metadata, result.text);
                imported++;
                emit logMessage(QString("  ✓ 成功 (%1 字符)").arg(result.text.length()));
            } else {
                skipped++;
                emit logMessage("  ⚠ 内容为空");
            }

        } catch (const std::exception& e) {
            failed++;
            emit logMessage(QString("  ✗ 失败: %1").arg(e.what()));
        }
    }

    emit logMessage(QString("\n导入完成: %1 成功, %2 失败, %3 跳过")
                        .arg(imported).arg(failed).arg(skipped));
    emit finished(imported, failed, skipped);
}

// ========== ImportDialog Implementation ==========

ImportDialog::ImportDialog(std::shared_ptr<XapianDatabase> database,
                           std::shared_ptr<XapianIndexer> indexer,
                           std::shared_ptr<ParserManager> processor,
                           QWidget* parent)
    : QDialog(parent)
    , m_database(std::move(database))
    , m_indexer(std::move(indexer))
    , m_processor(std::move(processor))
    , m_workerThread(nullptr)
    , m_worker(nullptr)
    , m_importedCount(0)
{
    setupUI();
}

void ImportDialog::setupUI()
{
    setWindowTitle(tr("导入文档"));
    resize(700, 500);
    setMinimumSize(500, 400);

    auto* layout = new QVBoxLayout(this);

    // Title
    auto* titleLabel = new QLabel(tr("<b>选择要导入索引的文件或文件夹</b>"), this);
    layout->addWidget(titleLabel);

    // File selection buttons
    auto* btnLayout = new QHBoxLayout();
    m_selectFilesBtn = new QPushButton(tr("选择文件..."), this);
    m_selectFolderBtn = new QPushButton(tr("选择文件夹..."), this);

    m_startBtn = new QPushButton(tr("开始导入"), this);
    m_startBtn->setEnabled(false);
    m_startBtn->setStyleSheet(
        "QPushButton { background-color: #4CAF50; color: white; border: none; "
        "border-radius: 4px; padding: 8px 16px; font-size: 14px; }"
        "QPushButton:hover { background-color: #388E3C; }"
        "QPushButton:disabled { background-color: #ccc; color: #999; }");

    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_cancelBtn->setEnabled(false);

    btnLayout->addWidget(m_selectFilesBtn);
    btnLayout->addWidget(m_selectFolderBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);
    layout->addLayout(btnLayout);

    // File list
    auto* fileListLabel = new QLabel(tr("待导入文件:"), this);
    layout->addWidget(fileListLabel);

    m_fileList = new QTreeWidget(this);
    m_fileList->setColumnCount(3);
    m_fileList->setHeaderLabels({tr("文件名"), tr("路径"), tr("大小")});
    m_fileList->setRootIsDecorated(false);
    m_fileList->setAlternatingRowColors(true);
    m_fileList->header()->setStretchLastSection(true);
    layout->addWidget(m_fileList, 1);

    // Progress
    m_progressLabel = new QLabel(this);
    layout->addWidget(m_progressLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar);

    // Log view
    auto* logLabel = new QLabel(tr("导入日志:"), this);
    layout->addWidget(logLabel);

    m_logView = new QTextEdit(this);
    m_logView->setReadOnly(true);
    m_logView->setMaximumHeight(150);
    m_logView->setStyleSheet(
        "QTextEdit { background-color: #1e1e1e; color: #d4d4d4; "
        "font-family: 'monospace'; font-size: 11px; }");
    layout->addWidget(m_logView);

    // Connections
    connect(m_selectFilesBtn, &QPushButton::clicked, this, &ImportDialog::onSelectFiles);
    connect(m_selectFolderBtn, &QPushButton::clicked, this, &ImportDialog::onSelectFolder);
    connect(m_startBtn, &QPushButton::clicked, this, &ImportDialog::onStartImport);
    connect(m_cancelBtn, &QPushButton::clicked, this, &ImportDialog::onCancelImport);
}

void ImportDialog::collectFiles(const QStringList& paths)
{
    QStringList supportedExts = m_processor->supportedExtensions();
    QStringList imageExts = {"png", "jpg", "jpeg", "gif", "bmp", "tiff", "tif", "webp"};
    supportedExts << imageExts;

    for (const auto& path : paths) {
        QFileInfo fi(path);
        if (fi.isDir()) {
            QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo fileInfo = it.fileInfo();
                QString ext = fileInfo.suffix().toLower();
                if (supportedExts.contains(ext)) {
                    m_filesToImport.append(fileInfo.absoluteFilePath());
                }
            }
        } else if (fi.isFile()) {
            m_filesToImport.append(fi.absoluteFilePath());
        }
    }

    // Update file list display
    m_fileList->clear();
    for (const auto& f : m_filesToImport) {
        QFileInfo fi(f);
        auto* item = new QTreeWidgetItem(m_fileList);
        item->setText(0, fi.fileName());
        item->setText(1, fi.absolutePath());
        item->setText(2, QString::number(fi.size()));
    }

    m_startBtn->setEnabled(!m_filesToImport.isEmpty());
    m_progressLabel->setText(tr("已选择 %1 个文件").arg(m_filesToImport.size()));
}

void ImportDialog::onSelectFiles()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        tr("选择要导入的文件"), QString(),
        tr("所有支持的格式 (*.txt *.md *.pdf *.docx *.csv *.xml *.json "
           "*.py *.js *.ts *.cpp *.h *.java *.html *.css);;"
           "所有文件 (*)"));
    if (!files.isEmpty()) {
        collectFiles(files);
    }
}

void ImportDialog::onSelectFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("选择要导入的文件夹"), QString());
    if (!dir.isEmpty()) {
        QStringList dirs;
        dirs << dir;
        collectFiles(dirs);
    }
}

void ImportDialog::onStartImport()
{
    if (m_filesToImport.isEmpty()) return;

    // Disable UI
    m_selectFilesBtn->setEnabled(false);
    m_selectFolderBtn->setEnabled(false);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(true);

    m_progressBar->setValue(0);
    m_importStartTime = QDateTime::currentSecsSinceEpoch();

    // Create worker and thread
    m_workerThread = new QThread(this);
    m_worker = new ImportWorker(m_database, m_indexer, m_processor, m_filesToImport);
    m_worker->moveToThread(m_workerThread);

    connect(m_workerThread, &QThread::started, m_worker, &ImportWorker::process);
    connect(m_worker, &ImportWorker::finished, this, &ImportDialog::onImportFinished);
    connect(m_worker, &ImportWorker::finished, m_workerThread, &QThread::quit);
    connect(m_worker, &ImportWorker::progressUpdated, this, &ImportDialog::onProgressUpdated);
    connect(m_worker, &ImportWorker::logMessage, this, &ImportDialog::onLogMessage);
    connect(m_worker, &ImportWorker::importError, this, &ImportDialog::onImportError);

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
    connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);

    m_workerThread->start();
    emit importStarted();
}

void ImportDialog::onCancelImport()
{
    if (m_worker) {
        m_worker->thread()->requestInterruption();
    }
    m_cancelBtn->setEnabled(false);
    m_logView->append("正在取消...");
}

void ImportDialog::onProgressUpdated(int current, int total, const QString& currentFile)
{
    int percent = total > 0 ? (current * 100 / total) : 0;
    m_progressBar->setValue(percent);
    m_progressLabel->setText(tr("正在处理: %1/%2 - %3").arg(current).arg(total).arg(currentFile));
}

void ImportDialog::onLogMessage(const QString& message)
{
    m_logView->append(message);
    // Auto-scroll to bottom
    QScrollBar* sb = m_logView->verticalScrollBar();
    if (sb) sb->setValue(sb->maximum());
}

void ImportDialog::onImportFinished(int imported, int failed, int skipped)
{
    m_importedCount = imported;
    m_progressBar->setValue(100);
    qint64 elapsed = m_importStartTime > 0
        ? QDateTime::currentSecsSinceEpoch() - m_importStartTime
        : 0;
    m_importStartTime = 0;
    QString timeInfo = elapsed > 0 ? tr(" (用时 %1 秒)").arg(elapsed) : QString();
    m_progressLabel->setText(tr("导入完成: %1 成功, %2 失败, %3 跳过%4")
                                .arg(imported).arg(failed).arg(skipped).arg(timeInfo));

    // Re-enable UI
    m_selectFilesBtn->setEnabled(true);
    m_selectFolderBtn->setEnabled(true);
    m_startBtn->setEnabled(false);
    m_cancelBtn->setEnabled(false);

    m_filesToImport.clear();

    emit importCompleted(imported, failed, skipped);
}

void ImportDialog::onImportError(const QString& error)
{
    m_logView->append(tr("错误: %1").arg(error));
    QMessageBox::critical(this, tr("导入错误"), error);
}
