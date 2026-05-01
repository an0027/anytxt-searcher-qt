/*
 * import_dialog.h - 导入对话框

功能说明：批量导入文档的对话框，支持文件和文件夹选择、
索引进度显示和日志输出。
 */

#ifndef ANYTXT_IMPORT_DIALOG_H
#define ANYTXT_IMPORT_DIALOG_H

#include <QDialog>
#include <QTreeWidget>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThread>
#include <memory>

class XapianDatabase;
class XapianIndexer;
class ParserManager;
class DocumentProcessor;

class ImportWorker : public QObject {
    Q_OBJECT
public:
    ImportWorker(std::shared_ptr<XapianDatabase> db,
                 std::shared_ptr<XapianIndexer> indexer,
                 std::shared_ptr<ParserManager> processor,
                 const QStringList& files);
    ~ImportWorker() override = default;

public slots:
    void process();

signals:
    void progressUpdated(int current, int total, const QString& currentFile);
    void logMessage(const QString& message);
    void finished(int imported, int failed, int skipped);
    void importError(const QString& error);

private:
    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<ParserManager> m_processor;
    QStringList m_files;
    volatile bool m_cancelled = false;
};

class ImportDialog : public QDialog {
    Q_OBJECT
public:
    ImportDialog(std::shared_ptr<XapianDatabase> database,
                 std::shared_ptr<XapianIndexer> indexer,
                 std::shared_ptr<ParserManager> processor,
                 QWidget* parent = nullptr);
    ~ImportDialog() override = default;

    int importedCount() const { return m_importedCount; }

signals:
    void importStarted();
    void importCompleted(int imported, int failed, int skipped);

private slots:
    void onSelectFiles();
    void onSelectFolder();
    void onStartImport();
    void onCancelImport();
    void onProgressUpdated(int current, int total, const QString& currentFile);
    void onLogMessage(const QString& message);
    void onImportFinished(int imported, int failed, int skipped);
    void onImportError(const QString& error);

private:
    void setupUI();
    void collectFiles(const QStringList& paths);

    std::shared_ptr<XapianDatabase> m_database;
    std::shared_ptr<XapianIndexer> m_indexer;
    std::shared_ptr<ParserManager> m_processor;

    QStringList m_filesToImport;

    QTreeWidget* m_fileList;
    QPushButton* m_selectFilesBtn;
    QPushButton* m_selectFolderBtn;
    QPushButton* m_startBtn;
    QPushButton* m_cancelBtn;
    QProgressBar* m_progressBar;
    QLabel* m_progressLabel;
    QTextEdit* m_logView;

    QThread* m_workerThread;
    ImportWorker* m_worker;

    int m_importedCount;
};

#endif // ANYTXT_IMPORT_DIALOG_H
