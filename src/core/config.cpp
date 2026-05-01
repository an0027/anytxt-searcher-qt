/*
 * config.cpp - AnyTXT Searcher 配置管理实现
 *
 * 功能说明：实现索引配置的加载、保存和数据库目录创建功能。
 *          使用 QSettings 进行配置持久化，使用 QStandardPaths
 *          确定默认数据存储位置。
 */

#include "core/config.h"
#include <QStandardPaths>
#include <QSettings>
#include <QDebug>

/**
 * @brief 构造函数：初始化默认数据库路径
 *
 * 默认数据库路径位于系统标准应用数据目录下的 "index" 子目录。
 * 例如 Linux 下为 ~/.local/share/AnyTXT/index，
 * Windows 下为 C:/Users/<user>/AppData/Local/AnyTXT/index。
 */
IndexConfig::IndexConfig()
{
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    dbPath = dataDir + "/index";
}

/**
 * @brief 获取数据库路径
 * @return 当前配置的数据库目录路径
 */
QString IndexConfig::getDatabasePath() const
{
    return dbPath;
}

/**
 * @brief 确保数据库目录存在
 *
 * 检查数据库目录是否存在，如果不存在则尝试创建。
 * 创建操作会记录调试日志。
 *
 * @return true 表示目录就绪，false 表示创建失败
 */
bool IndexConfig::ensureDatabaseDirectory() const
{
    QDir dir(dbPath);
    if (!dir.exists()) {
        qDebug() << "Creating database directory:" << dbPath;
        bool ok = dir.mkpath(".");
        if (!ok) {
            qWarning() << "Failed to create database directory:" << dbPath;
        }
        return ok;
    }
    return true;
}

/**
 * @brief 保存当前配置到持久化存储
 *
 * 使用 QSettings 将所有索引配置写入 "index" 分组中，
 * 包括路径、词干语言、批处理大小、拼写检查、文件监控等设置。
 */
void IndexConfig::save() const
{
    QSettings settings;
    settings.beginGroup("index");
    settings.setValue("path", dbPath);
    settings.setValue("stemLanguage", stemLanguage);
    settings.setValue("batchSize", batchSize);
    settings.setValue("enableSpelling", enableSpelling);
    settings.setValue("enableFileWatching", enableFileWatching);
    settings.setValue("autoIndexOnStart", autoIndexOnStart);
    settings.setValue("watchIntervalMs", watchIntervalMs);
    settings.setValue("watchedFolders", watchedFolders);
    settings.endGroup();
}

/**
 * @brief 从持久化存储加载配置
 *
 * 从 QSettings 的 "index" 分组中读取所有配置项。
 * 如果某配置项不存在，则使用当前成员变量的值作为默认值。
 */
void IndexConfig::load()
{
    QSettings settings;
    settings.beginGroup("index");
    dbPath = settings.value("path", dbPath).toString();
    stemLanguage = settings.value("stemLanguage", "english").toString();
    batchSize = settings.value("batchSize", 100).toInt();
    enableSpelling = settings.value("enableSpelling", false).toBool();
    enableFileWatching = settings.value("enableFileWatching", false).toBool();
    autoIndexOnStart = settings.value("autoIndexOnStart", false).toBool();
    watchIntervalMs = settings.value("watchIntervalMs", 2000).toInt();
    watchedFolders = settings.value("watchedFolders", QStringList()).toStringList();
    settings.endGroup();
}

/**
 * @brief 获取默认配置
 * @return 使用所有默认值的 IndexConfig 对象
 */
IndexConfig IndexConfig::defaultConfig()
{
    IndexConfig config;
    return config;
}

/**
 * @brief 从设置文件创建配置
 *
 * 加载指定路径的配置，然后返回配置对象。
 * 当前实现中 settingsPath 参数暂未使用（预留接口）。
 *
 * @param settingsPath 设置文件路径（当前未使用）
 * @return 加载后的 IndexConfig 对象
 */
IndexConfig IndexConfig::fromSettings(const QString& settingsPath)
{
    Q_UNUSED(settingsPath);
    IndexConfig config;
    config.load();
    return config;
}
