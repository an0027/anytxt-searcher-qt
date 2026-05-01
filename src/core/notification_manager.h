/*
 * notification_manager.h - 通知管理模块
 *
 * 功能说明：管理系统托盘通知和机器人（Webhook）推送。
 *          支持 Feishu/企业微信等兼容 JSON POST 的 Webhook。
 */

#ifndef ANYTXT_NOTIFICATION_MANAGER_H
#define ANYTXT_NOTIFICATION_MANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QSettings>

class NotificationManager : public QObject {
    Q_OBJECT
public:
    explicit NotificationManager(QSystemTrayIcon* trayIcon, QObject* parent = nullptr);
    ~NotificationManager() override = default;

    /// 配置 Webhook URL
    void setWebhookUrl(const QString& url);
    QString webhookUrl() const;

    // ── 通知事件 ──
    void notifyIndexComplete(int indexed, int failed, qint64 elapsedSec);
    void notifyIndexProgress(int indexed, int total, const QString& currentFile);
    void notifySearchComplete(int results, qint64 elapsedMs);
    void notifyIndexError(const QString& error);
    void notifyFileWatchNewFiles(int count);
    void notifyInfo(const QString& title, const QString& message);

private:
    void sendTray(const QString& title, const QString& message,
                  QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information);
    void sendWebhook(const QString& title, const QString& message);

    QSystemTrayIcon* m_trayIcon;
    QNetworkAccessManager* m_networkManager;
    QString m_webhookUrl;
};

#endif // ANYTXT_NOTIFICATION_MANAGER_H
