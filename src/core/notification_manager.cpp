/*
 * notification_manager.cpp - 通知管理实现
 */

#include "notification_manager.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>

NotificationManager::NotificationManager(QSystemTrayIcon* trayIcon, QObject* parent)
    : QObject(parent)
    , m_trayIcon(trayIcon)
{
}

void NotificationManager::sendTray(const QString& title, const QString& message,
                                   QSystemTrayIcon::MessageIcon icon)
{
    if (!m_trayIcon || !m_trayIcon->isVisible()) return;
    m_trayIcon->showMessage(title, message, icon, 5000);
}

void NotificationManager::notifyIndexComplete(int indexed, int failed, qint64 elapsedSec)
{
    QString title = tr("索引完成");
    QString msg = tr("已索引 %1 个文档").arg(indexed);
    if (failed > 0) msg += tr("，%1 个失败").arg(failed);
    msg += tr("，用时 %1 秒").arg(elapsedSec);
    sendTray(title, msg, QSystemTrayIcon::Information);
}

void NotificationManager::notifySearchComplete(int results)
{
    if (results > 0) {
        sendTray(tr("搜索完成"), tr("找到 %1 个结果").arg(results), QSystemTrayIcon::Information);
    }
}

void NotificationManager::notifyIndexError(const QString& error)
{
    sendTray(tr("索引错误"), error, QSystemTrayIcon::Critical);
}

void NotificationManager::notifyInfo(const QString& title, const QString& message)
{
    sendTray(title, message, QSystemTrayIcon::Information);
}
