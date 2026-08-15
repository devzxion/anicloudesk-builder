#include "AppRuntime.h"

#include "Database.h"

#include <QApplication>
#include <QClipboard>
#include <QIcon>
#include <QSystemTrayIcon>
#include <QUrl>

AppRuntime::AppRuntime(Database *database, QObject *parent)
  : QObject(parent), m_database(database), m_settings() {
  m_route = m_settings.value(QStringLiteral("navigation/lastRoute"), QStringLiteral("home")).toString();
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/qt/qml/AniCloud/resources/icon.png")), this);
    m_tray->setToolTip(QStringLiteral("AniCloud"));
    m_tray->show();
  }
  refreshLocalHistory();
}

AppRuntime::~AppRuntime() = default;

QString AppRuntime::audioPreference() const { return m_settings.value(QStringLiteral("playback/audio"), QStringLiteral("sub")).toString(); }
QString AppRuntime::playbackQuality() const { return m_settings.value(QStringLiteral("playback/quality"), QStringLiteral("auto")).toString(); }
int AppRuntime::downloadQuality() const { return m_settings.value(QStringLiteral("downloads/quality"), 1080).toInt(); }
bool AppRuntime::allowMeteredDownloads() const { return m_settings.value(QStringLiteral("downloads/allowMetered"), false).toBool(); }
bool AppRuntime::notificationsEnabled() const { return m_settings.value(QStringLiteral("notifications/enabled"), true).toBool(); }

void AppRuntime::setRoute(const QString &route) {
  if (route.isEmpty() || m_route == route) return;
  if (route == QStringLiteral("auth") && m_route != QStringLiteral("auth")) {
    m_pendingRoute = m_route;
    emit pendingRouteChanged();
  }
  m_route = route; m_settings.setValue(QStringLiteral("navigation/lastRoute"), route); emit routeChanged();
}
void AppRuntime::setAudioPreference(const QString &value) { m_settings.setValue(QStringLiteral("playback/audio"), value); emit preferencesChanged(); }
void AppRuntime::setPlaybackQuality(const QString &value) { m_settings.setValue(QStringLiteral("playback/quality"), value); emit preferencesChanged(); }
void AppRuntime::setDownloadQuality(int value) { m_settings.setValue(QStringLiteral("downloads/quality"), value); emit preferencesChanged(); }
void AppRuntime::setAllowMeteredDownloads(bool value) { m_settings.setValue(QStringLiteral("downloads/allowMetered"), value); emit preferencesChanged(); }
void AppRuntime::setNotificationsEnabled(bool value) { m_settings.setValue(QStringLiteral("notifications/enabled"), value); emit preferencesChanged(); }

void AppRuntime::refreshLocalHistory() { m_localHistory = m_database->localHistory(200); emit localHistoryChanged(); }
void AppRuntime::deleteLocalHistory(const QString &animeId, const QString &episodeId) {
  if (m_database->removeLocalHistory(animeId, episodeId)) refreshLocalHistory();
}

void AppRuntime::handleDeepLink(const QString &value) {
  const QUrl url(value); if (url.scheme() != QStringLiteral("anicloud")) return;
  QString route = url.host();
  if (!url.path().isEmpty() && url.path() != QStringLiteral("/")) route += url.path();
  if (route.isEmpty()) route = QStringLiteral("home");
  setRoute(route);
  emit deepLinkReceived(route);
}

void AppRuntime::restorePendingRoute() {
  if (m_pendingRoute.isEmpty()) return; setRoute(m_pendingRoute); m_pendingRoute.clear(); emit pendingRouteChanged();
}

void AppRuntime::showNotification(const QString &title, const QString &message) {
  if (!notificationsEnabled() || !m_tray) return; m_tray->showMessage(title, message, QSystemTrayIcon::Information, 8000);
}

QVariant AppRuntime::windowValue(const QString &key, const QVariant &fallback) const { return m_settings.value(QStringLiteral("window/") + key, fallback); }
void AppRuntime::setWindowValue(const QString &key, const QVariant &value) { m_settings.setValue(QStringLiteral("window/") + key, value); }
void AppRuntime::copyText(const QString &text) { QApplication::clipboard()->setText(text); }
