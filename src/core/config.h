/*
 * config.h - AnyTXT Searcher 配置管理模块
 *
 * 功能说明：提供索引数据库的配置管理，包括数据库路径、词干语言、
 *          批处理大小、拼写检查开关以及智能文件监控等设置。
 *          支持配置的保存、加载和默认值获取。
 *
 * 作者：AnyTXT Searcher Team
 */

#ifndef ANYTXT_CONFIG_H
#define ANYTXT_CONFIG_H

#include <QString>
#include <QDir>
#include <QStringList>

/**
 * @brief 索引配置类
 *
 * 管理全文搜索引擎的索引配置参数，包括数据库存储路径、索引选项、
 * 文件监控设置等。支持将配置持久化到系统注册表/配置文件，
 * 并可从存储中重新加载。
 */
class IndexConfig {
public:
    IndexConfig();

    // ---- 数据库/索引核心设置 ----
    QString dbPath;              ///< Xapian 数据库的存储路径
    QString stemLanguage = "english"; ///< 词干提取语言（默认英语）
    int batchSize = 100;         ///< 批量索引时的每批文档数
    bool enableSpelling = false; ///< 是否启用拼写检查建议

    // ---- 智能索引设置 ----
    bool enableFileWatching = false; ///< 是否启用文件变更监控
    QStringList watchedFolders;      ///< 被监控的文件夹路径列表
    int watchIntervalMs = 2000;      ///< 文件监控轮询间隔（毫秒）
    bool autoIndexOnStart = false;   ///< 是否在启动时自动索引

    /**
     * @brief 获取数据库完整路径
     * @return 数据库目录的绝对路径
     */
    QString getDatabasePath() const;

    /**
     * @brief 确保数据库目录存在，不存在则创建
     * @return true 表示目录存在或创建成功，false 表示创建失败
     */
    bool ensureDatabaseDirectory() const;

    /**
     * @brief 将当前配置保存到持久化存储
     */
    void save() const;

    /**
     * @brief 从持久化存储中加载配置
     */
    void load();

    /**
     * @brief 获取默认配置实例
     * @return 使用默认值的 IndexConfig 对象
     */
    static IndexConfig defaultConfig();

    /**
     * @brief 从指定设置文件路径加载配置
     * @param settingsPath 设置文件路径（可选，为空则使用默认位置）
     * @return 加载后的 IndexConfig 对象
     */
    static IndexConfig fromSettings(const QString& settingsPath = QString());
};

#endif // ANYTXT_CONFIG_H
