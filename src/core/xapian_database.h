/*
 * xapian_database.h - AnyTXT Searcher Xapian 数据库封装类
 *
 * 功能说明：封装 Xapian 数据库的打开、关闭、创建和基础查询操作。
 *          支持读写模式和只读模式，提供线程安全锁机制，
 *          以及数据库信息查询和压缩功能。
 */

#ifndef ANYTXT_XAPIAN_DATABASE_H
#define ANYTXT_XAPIAN_DATABASE_H

#include <xapian.h>
#include <QString>
#include <QMap>
#include <QMutex>

/**
 * @brief Xapian 数据库封装类
 *
 * 对 Xapian 数据库进行面向对象封装，统一管理只读数据库和可写数据库。
 * 支持线程安全的互斥锁访问，提供创建、打开、关闭、压缩等操作。
 * 内部维护只读 Xapian::Database 和可写 Xapian::WritableDatabase 两个实例，
 * 以满足读写分离的需求。
 */
class XapianDatabase {
public:
    XapianDatabase();
    virtual ~XapianDatabase();

    /**
     * @brief 创建或打开数据库（读写模式）
     * @param path 数据库存储路径
     * @return true 表示操作成功
     * @throws DatabaseError 创建失败时抛出异常
     */
    bool create(const QString& path);

    /**
     * @brief 打开已有数据库
     * @param path 数据库路径
     * @param writable true 以读写模式打开，false 以只读模式打开
     * @return true 表示操作成功
     * @throws DatabaseError 打开失败时抛出异常
     */
    bool open(const QString& path, bool writable = false);

    /**
     * @brief 关闭数据库连接
     */
    void close();

    /**
     * @brief 检查数据库是否已打开
     * @return true 表示数据库已打开
     */
    bool isOpen() const;

    /**
     * @brief 获取只读数据库引用
     * @return 只读 Xapian::Database 引用
     * @throws DatabaseError 如果数据库未打开
     */
    Xapian::Database& getDatabase();
    const Xapian::Database& getDatabase() const;

    /**
     * @brief 获取可写数据库引用
     * @return 可写 Xapian::WritableDatabase 引用
     * @throws DatabaseError 如果可写数据库未打开
     */
    Xapian::WritableDatabase& getWritableDatabase();

    /**
     * @brief 压缩数据库到指定路径
     * @param outputPath 压缩后的输出路径
     * @return true 表示压缩成功
     */
    bool compact(const QString& outputPath);

    /**
     * @brief 刷新只读数据库（在写入操作后调用以同步最新数据）
     * @return true 表示刷新成功
     */
    bool refresh();

    /**
     * @brief 获取数据库信息（文档数、后端类型等）
     * @return 包含各项信息的键值对映射
     */
    QMap<QString, QString> getInfo() const;

    /**
     * @brief 获取文档总数
     * @return 数据库中的文档数量，数据库未打开时返回 0
     */
    int getDocumentCount() const;

    // ---- 线程安全相关 ----

    /**
     * @brief 手动加锁（允许外部代码执行原子操作序列）
     */
    void lock();

    /**
     * @brief 手动解锁
     */
    void unlock();

private:
    /**
     * @brief 内部关闭方法（不重新获取互斥锁）
     * 供在已持有锁的上下文中调用。
     */
    void privateClose();

    Xapian::Database m_database;                ///< 只读 Xapian 数据库实例
    Xapian::WritableDatabase* m_writableDb = nullptr; ///< 可写数据库实例指针
    QString m_path;                             ///< 数据库文件路径
    bool m_isOpen = false;                      ///< 数据库是否已打开
    bool m_isWritable = false;                  ///< 是否以可写模式打开
    mutable QMutex m_mutex;                     ///< 互斥锁，保证线程安全
};

#endif // ANYTXT_XAPIAN_DATABASE_H
