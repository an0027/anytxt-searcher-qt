/*
 * xapian_database.cpp - AnyTXT Searcher Xapian 数据库封装实现
 *
 * 功能说明：实现 Xapian 数据库的创建、打开、关闭、压缩、刷新
 *          和信息查询等功能。管理只读和可写两个数据库实例以
 *          支持并发读写操作。
 */

#include "core/xapian_database.h"
#include "core/exceptions.h"
#include <QDebug>
#include <QDir>

/**
 * @brief 默认构造函数
 */
XapianDatabase::XapianDatabase()
{
}

/**
 * @brief 析构函数：自动关闭数据库连接
 */
XapianDatabase::~XapianDatabase()
{
    close();
}

/**
 * @brief 内部关闭（无锁版本）
 *
 * 依次关闭可写数据库和只读数据库，清理所有资源。
 * 调用者需保证已持有互斥锁。
 */
void XapianDatabase::privateClose()
{
    // 先关闭可写数据库（提交未完成的事务）
    if (m_writableDb) {
        try {
            m_writableDb->commit();
            m_writableDb->close();
        } catch (const Xapian::Error& e) {
            qWarning() << "Error closing writable database:" << e.get_description().c_str();
        }
        delete m_writableDb;
        m_writableDb = nullptr;
    }
    // 关闭只读数据库
    if (m_isOpen) {
        try {
            m_database.close();
        } catch (const Xapian::Error& e) {
            qWarning() << "Error closing database:" << e.get_description().c_str();
        }
    }
    m_isOpen = false;
    m_isWritable = false;
    m_path.clear();
    qDebug() << "Xapian database closed";
}

/**
 * @brief 创建新数据库或以读写模式打开已有数据库
 *
 * 如果指定路径的目录不存在则自动创建。
 * 同时打开只读数据库实例以便并发搜索使用。
 *
 * @param path 数据库存储路径
 * @return true 表示操作成功
 * @throws DatabaseError 创建失败时抛出异常
 */
bool XapianDatabase::create(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    try {
        privateClose();
        // 确保目录存在
        QDir dir(path);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        // 以 CREATE_OR_OPEN 模式打开可写数据库
        m_writableDb = new Xapian::WritableDatabase(path.toStdString(),
            Xapian::DB_CREATE_OR_OPEN);
        m_path = path;
        m_isOpen = true;
        m_isWritable = true;
        // 同时打开只读数据库用于并发读取
        m_database = Xapian::Database(path.toStdString());
        qDebug() << "Xapian database created/opened at:" << path;
        return true;
    } catch (const Xapian::Error& e) {
        QString err = QString("Failed to create database: %1").arg(e.get_description().c_str());
        qWarning() << err;
        m_isOpen = false;
        m_isWritable = false;
        m_writableDb = nullptr;
        throw DatabaseError(err);
    }
}

/**
 * @brief 以指定模式打开已有数据库
 *
 * @param path 数据库路径
 * @param writable true 为读写模式，false 为只读模式
 * @return true 表示操作成功
 * @throws DatabaseError 打开失败时抛出异常
 */
bool XapianDatabase::open(const QString& path, bool writable)
{
    QMutexLocker locker(&m_mutex);
    try {
        privateClose();
        if (writable) {
            // 读写模式：以 DB_OPEN 模式打开已有数据库
            m_writableDb = new Xapian::WritableDatabase(path.toStdString(),
                Xapian::DB_OPEN);
            // 同时打开只读数据库用于并发搜索
            m_database = Xapian::Database(path.toStdString());
            m_isWritable = true;
        } else {
            // 只读模式
            m_database = Xapian::Database(path.toStdString());
            m_writableDb = nullptr;
            m_isWritable = false;
        }
        m_path = path;
        m_isOpen = true;
        qDebug() << "Xapian database opened (writable=" << writable << ") at:" << path;
        return true;
    } catch (const Xapian::Error& e) {
        QString err = QString("Failed to open database: %1").arg(e.get_description().c_str());
        qWarning() << err;
        m_isOpen = false;
        m_isWritable = false;
        m_writableDb = nullptr;
        throw DatabaseError(err);
    }
}

/**
 * @brief 关闭数据库连接（线程安全）
 */
void XapianDatabase::close()
{
    QMutexLocker locker(&m_mutex);
    privateClose();
}

/**
 * @brief 检查数据库是否已打开
 * @return true 表示数据库已打开
 */
bool XapianDatabase::isOpen() const
{
    return m_isOpen;
}

/**
 * @brief 获取只读数据库的非 const 引用
 * @return Xapian::Database& 引用
 * @throws DatabaseError 如果数据库未打开
 */
Xapian::Database& XapianDatabase::getDatabase()
{
    if (!m_isOpen) {
        throw DatabaseError("Database is not open");
    }
    return m_database;
}

/**
 * @brief 获取只读数据库的 const 引用
 * @return const Xapian::Database& 引用
 * @throws DatabaseError 如果数据库未打开
 */
const Xapian::Database& XapianDatabase::getDatabase() const
{
    if (!m_isOpen) {
        throw DatabaseError("Database is not open");
    }
    return m_database;
}

/**
 * @brief 获取可写数据库引用
 * @return Xapian::WritableDatabase& 引用
 * @throws DatabaseError 如果可写数据库未打开
 */
Xapian::WritableDatabase& XapianDatabase::getWritableDatabase()
{
    if (!m_isOpen || !m_isWritable || !m_writableDb) {
        throw DatabaseError("Writable database is not open");
    }
    return *m_writableDb;
}

/**
 * @brief 压缩数据库到指定输出路径
 *
 * 先提交可写数据库的未决更改，然后通过只读方式创建源数据库
 * 后进行压缩操作。
 *
 * @param outputPath 压缩后的输出路径
 * @return true 表示压缩成功
 */
bool XapianDatabase::compact(const QString& outputPath)
{
    QMutexLocker locker(&m_mutex);
    try {
        // 先刷写未决的写入
        if (m_isOpen) {
            if (m_writableDb) {
                m_writableDb->commit();
            }
        }
        // 通过只读数据库执行压缩
        Xapian::Database sourceDb(m_path.toStdString());
        sourceDb.compact(outputPath.toStdString());
        qDebug() << "Database compacted to:" << outputPath;
        return true;
    } catch (const Xapian::Error& e) {
        qWarning() << "Failed to compact database:" << e.get_description().c_str();
        return false;
    }
}

/**
 * @brief 获取数据库详细信息
 *
 * 包括文档数量、后端类型、最后文档 ID、路径、状态等信息。
 *
 * @return 信息键值对映射
 */
QMap<QString, QString> XapianDatabase::getInfo() const
{
    QMap<QString, QString> info;
    if (!m_isOpen) {
        info["status"] = "closed";
        return info;
    }
    try {
        int docCount = 0;
        QString backend, uuid;
        Xapian::doccount lastDocId = 0;

        // 根据可写/只读模式选择不同的数据源
        if (m_isWritable && m_writableDb) {
            docCount = m_writableDb->get_doccount();
            backend = m_writableDb->get_description().c_str();
            lastDocId = m_writableDb->get_lastdocid();
        } else {
            docCount = m_database.get_doccount();
            backend = m_database.get_description().c_str();
            lastDocId = m_database.get_lastdocid();
        }

        info["documentCount"] = QString::number(docCount);
        info["backend"] = backend;
        info["lastDocId"] = QString::number(lastDocId);
        info["path"] = m_path;
        info["status"] = "open";
        info["writable"] = m_isWritable ? "true" : "false";
    } catch (const Xapian::Error& e) {
        info["error"] = e.get_description().c_str();
    }
    return info;
}

/**
 * @brief 获取文档总数
 * @return 文档数量，数据库未打开时返回 0
 */
int XapianDatabase::getDocumentCount() const
{
    if (!m_isOpen) return 0;
    try {
        if (m_isWritable && m_writableDb) {
            return static_cast<int>(m_writableDb->get_doccount());
        }
        return static_cast<int>(m_database.get_doccount());
    } catch (const Xapian::Error&) {
        return 0;
    }
}

/**
 * @brief 手动加锁
 *
 * 允许外部代码获取锁后执行一系列原子操作。
 */
void XapianDatabase::lock()
{
    m_mutex.lock();
}

/**
 * @brief 手动解锁
 */
void XapianDatabase::unlock()
{
    m_mutex.unlock();
}

/**
 * @brief 刷新只读数据库
 *
 * 重新打开只读数据库以反映可写数据库的最新变更。
 *
 * @return true 表示刷新成功
 */
bool XapianDatabase::refresh()
{
    QMutexLocker locker(&m_mutex);
    // 当数据库已打开且路径非空时，执行 reopen
    if (!m_isOpen || !m_path.isEmpty()) {
        try {
            m_database.reopen();
            return true;
        } catch (const Xapian::Error& e) {
            qWarning() << "Failed to reopen database:" << e.get_description().c_str();
            return false;
        }
    }
    return false;
}
