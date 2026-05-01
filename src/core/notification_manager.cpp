/*
 * notification_manager.cpp - 通知管理实现
 */

#include "notification_manager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QApplication>
#include <QDateTime>
#include <QDebug>

NotificationManager::NotificationManager(QSystemTrayIcon* trayIcon, QObject* parent)
    : QObject(parent)
    , m_trayIcon(trayIcon)
    , m_networkManager(new QNetworkAccessManager(this))
{
    // Load webhook URL from settings
    QSettings settings;
    m_webhookUrl = settings.value("app/webhookUrl", "").toString();
}

void NotificationManager::setWebhookUrl(const QString& url)
{
    m_webhookUrl = url;
    QSettings settings;
    settings.setValue("app/webhookUrl", url);
}

QString NotificationManager::webhookUrl() const
{
    return m_webhookUrl;
}

// ── Tray notification ──
void NotificationManager::sendTray(const QString& title, const QString& message,
                                   QSystemTrayIcon::MessageIcon icon)
{
    if (!m_trayIcon || !m_trayIcon->isVisible()) return;
    m_trayIcon->showMessage(title, message, icon, 5000);
}

// ── Webhook (robot) push ──
void NotificationManager::sendWebhook(const QString& title, const QString& message)
{
    if (m_webhookUrl.isEmpty()) return;
    if (!m_webhookUrl.startsWith("http://") && !m_webhookUrl.startsWith("https://")) return;

    QJsonObject content;
    content["text"] = QString("[AnyTXT] %1\n%2").arg(title, message);

    QJsonObject body;
    body["msg_type"] = "text";
    body["content"] = content;

    QNetworkRequest request(QUrl(m_webhookUrl));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(10000);

    QByteArray data = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply* reply = m_networkManager->post(request, data);

    connect(reply, &QNetworkReply::finished, this, [reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Webhook send failed:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

// ── Event handlers ──
void NotificationManager::notifyIndexComplete(int indexed, int failed, qint64 elapsedSec)
{
    QString title = tr("索引完成");
    QString msg = tr("已索引 %1 个文档").arg(indexed);
    if (failed > 0) msg += tr("，%1 个失败").arg(failed);
    msg += tr("，用时 %1 秒").arg(elapsedSec);

    sendTray(title, msg, QSystemTrayIcon::Information);
    sendWebhook(title, msg);
}

void NotificationManager::notifyIndexProgress(int indexed, int total, const QString& /*currentFile*/)
{
    if (total <= 0) return;
    // Only send tray update for every 25% progress
    if (total >= 10 && indexed % (total / 4) == 0 && indexed > 0) {
        int pct = indexed * 100 / total;
        sendTray(tr("索引进度"), tr("%1 / %2 (%3%)").arg(indexed).arg(total).arg(pct));
    }
}

void NotificationManager::notifySearchComplete(int results, qint64 elapsedMs)
{
    Q_UNUSED(elapsedMs);
    // Only notify tray for non-empty results (brief)
    if (results > 0) {
        QString title = tr("搜索完成");
        QString msg = tr("找到 %1 个结果").arg(results);
        sendTray(title, msg, QSystemTrayIcon::Information);
    }
}

void NotificationManager::notifyIndexError(const QString& error)
{
    QString title = tr("索引错误");
    sendTray(title, error, QSystemTrayIcon::Critical);
    sendWebhook(title, error);
}

void NotificationManager::notifyFileWatchNewFiles(int count)
{
    if (count <= 0) return;
    QString title = tr("新文件检测");
    QString msg = tr("发现 %1 个新文件，正在编入索引").arg(count);
    sendTray(title, msg, QSystemTrayIcon::Information);
}

void NotificationManager::notifyInfo(const QString& title, const QString& message)
{
    sendTray(title, message, QSystemTrayIcon::Information);
}
