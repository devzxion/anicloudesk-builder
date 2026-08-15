#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>

class Database;
class QSystemTrayIcon;

class AppRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString route READ route WRITE setRoute NOTIFY routeChanged)
  Q_PROPERTY(QVariantList localHistory READ localHistory NOTIFY localHistoryChanged)
  Q_PROPERTY(QString audioPreference READ audioPreference WRITE setAudioPreference NOTIFY preferencesChanged)
  Q_PROPERTY(QString playbackQuality READ playbackQuality WRITE setPlaybackQuality NOTIFY preferencesChanged)
  Q_PROPERTY(int downloadQuality READ downloadQuality WRITE setDownloadQuality NOTIFY preferencesChanged)
  Q_PROPERTY(bool allowMeteredDownloads READ allowMeteredDownloads WRITE setAllowMeteredDownloads NOTIFY preferencesChanged)
  Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY preferencesChanged)
  Q_PROPERTY(QString pendingRoute READ pendingRoute NOTIFY pendingRouteChanged)

public:
  explicit AppRuntime(Database *database, QObject *parent = nullptr);
  ~AppRuntime() override;

  [[nodiscard]] QString route() const { return m_route; }
  [[nodiscard]] QVariantList localHistory() const { return m_localHistory; }
  [[nodiscard]] QString audioPreference() const;
  [[nodiscard]] QString playbackQuality() const;
  [[nodiscard]] int downloadQuality() const;
  [[nodiscard]] bool allowMeteredDownloads() const;
  [[nodiscard]] bool notificationsEnabled() const;
  [[nodiscard]] QString pendingRoute() const { return m_pendingRoute; }

  void setRoute(const QString &route);
  void setAudioPreference(const QString &value);
  void setPlaybackQuality(const QString &value);
  void setDownloadQuality(int value);
  void setAllowMeteredDownloads(bool value);
  void setNotificationsEnabled(bool value);

  Q_INVOKABLE void refreshLocalHistory();
  Q_INVOKABLE void deleteLocalHistory(const QString &animeId, const QString &episodeId = {});
  Q_INVOKABLE void handleDeepLink(const QString &url);
  Q_INVOKABLE void restorePendingRoute();
  Q_INVOKABLE void showNotification(const QString &title, const QString &message);
  Q_INVOKABLE QVariant windowValue(const QString &key, const QVariant &fallback = {}) const;
  Q_INVOKABLE void setWindowValue(const QString &key, const QVariant &value);
  Q_INVOKABLE void copyText(const QString &text);

signals:
  void routeChanged();
  void localHistoryChanged();
  void preferencesChanged();
  void pendingRouteChanged();
  void deepLinkReceived(const QString &route);

private:
  Database *m_database = nullptr;
  QSettings m_settings;
  QSystemTrayIcon *m_tray = nullptr;
  QString m_route = QStringLiteral("home");
  QString m_pendingRoute;
  QVariantList m_localHistory;
};
