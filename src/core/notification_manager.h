/*
 * notification_manager.h - 通知管理模块
 *
 * 功能说明：管理系统托盘通知。
 */

#ifndef ANYTXT_NOTIFICATION_MANAGER_H
#define ANYTXT_NOTIFICATION_MANAGER_H

#include <QObject>
#include <QSystemTrayIcon>

class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(QSystemTrayIcon* trayIcon, QObject* parent = nullptr);
    ~NotificationManager() override = default;

    void notifyIndexComplete(int indexed, int failed, qint64 elapsedSec);
    void notifySearchComplete(int results);
    void notifyIndexError(const QString& error);
    void notifyInfo(const QString& title, const QString& message);

private:
    void sendTray(const QString& title, const QString& message,
                  QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);

    QSystemTrayIcon* m_trayIcon;
};

#endif // ANYTXT_NOTIFICATION_MANAGER_H
