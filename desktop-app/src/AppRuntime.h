#pragma once

#include <QObject>
#include <QSettings>
#include <QVariantList>

class Database;
class QMenu;
class QSystemTrayIcon;

class AppRuntime final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString route READ route WRITE setRoute NOTIFY routeChanged)
  Q_PROPERTY(QVariantList localHistory READ localHistory NOTIFY localHistoryChanged)
  Q_PROPERTY(QString audioPreference READ audioPreference WRITE setAudioPreference NOTIFY preferencesChanged)
  Q_PROPERTY(QString playbackQuality READ playbackQuality WRITE setPlaybackQuality NOTIFY preferencesChanged)
  Q_PROPERTY(QString serverPreference READ serverPreference WRITE setServerPreference NOTIFY preferencesChanged)
  Q_PROPERTY(double captionScale READ captionScale WRITE setCaptionScale NOTIFY preferencesChanged)
  Q_PROPERTY(QString captionColor READ captionColor WRITE setCaptionColor NOTIFY preferencesChanged)
  Q_PROPERTY(double captionBackgroundOpacity READ captionBackgroundOpacity WRITE setCaptionBackgroundOpacity NOTIFY preferencesChanged)
  Q_PROPERTY(bool captionOutline READ captionOutline WRITE setCaptionOutline NOTIFY preferencesChanged)
  Q_PROPERTY(int downloadQuality READ downloadQuality WRITE setDownloadQuality NOTIFY preferencesChanged)
  Q_PROPERTY(bool allowMeteredDownloads READ allowMeteredDownloads WRITE setAllowMeteredDownloads NOTIFY preferencesChanged)
  Q_PROPERTY(bool notificationsEnabled READ notificationsEnabled WRITE setNotificationsEnabled NOTIFY preferencesChanged)
  Q_PROPERTY(bool startAtLogin READ startAtLogin WRITE setStartAtLogin NOTIFY preferencesChanged)
  Q_PROPERTY(bool backgroundLaunch READ backgroundLaunch NOTIFY backgroundLaunchChanged)
  Q_PROPERTY(bool trayAvailable READ trayAvailable CONSTANT)
  Q_PROPERTY(QString pendingRoute READ pendingRoute NOTIFY pendingRouteChanged)

public:
  explicit AppRuntime(Database *database, bool backgroundLaunch = false, QObject *parent = nullptr);
  ~AppRuntime() override;

  [[nodiscard]] QString route() const { return m_route; }
  [[nodiscard]] QVariantList localHistory() const { return m_localHistory; }
  [[nodiscard]] QString audioPreference() const;
  [[nodiscard]] QString playbackQuality() const;
  [[nodiscard]] QString serverPreference() const;
  [[nodiscard]] double captionScale() const;
  [[nodiscard]] QString captionColor() const;
  [[nodiscard]] double captionBackgroundOpacity() const;
  [[nodiscard]] bool captionOutline() const;
  [[nodiscard]] int downloadQuality() const;
  [[nodiscard]] bool allowMeteredDownloads() const;
  [[nodiscard]] bool notificationsEnabled() const;
  [[nodiscard]] bool startAtLogin() const;
  [[nodiscard]] bool backgroundLaunch() const { return m_backgroundLaunch; }
  [[nodiscard]] bool trayAvailable() const { return m_tray != nullptr; }
  [[nodiscard]] QString pendingRoute() const { return m_pendingRoute; }

  void setRoute(const QString &route);
  void setAudioPreference(const QString &value);
  void setPlaybackQuality(const QString &value);
  void setServerPreference(const QString &value);
  void setCaptionScale(double value);
  void setCaptionColor(const QString &value);
  void setCaptionBackgroundOpacity(double value);
  void setCaptionOutline(bool value);
  void setDownloadQuality(int value);
  void setAllowMeteredDownloads(bool value);
  void setNotificationsEnabled(bool value);
  void setStartAtLogin(bool value);

  Q_INVOKABLE void refreshLocalHistory();
  Q_INVOKABLE void deleteLocalHistory(const QString &animeId, const QString &episodeId = {});
  Q_INVOKABLE void handleDeepLink(const QString &url);
  Q_INVOKABLE void restorePendingRoute();
  Q_INVOKABLE void showNotification(const QString &title, const QString &message);
  Q_INVOKABLE void showBroadcastNotification(const QString &id, const QString &title,
                                               const QString &message, const QString &linkUrl = {});
  Q_INVOKABLE QString animeShareUrl(const QString &animeId) const;
  Q_INVOKABLE void shareAnime(const QString &animeId, const QString &title = {});
  Q_INVOKABLE void requestShow();
  void activateForeground(bool resetColdRoute = true);
  Q_INVOKABLE void quitApplication();
  Q_INVOKABLE QVariant windowValue(const QString &key, const QVariant &fallback = {}) const;
  Q_INVOKABLE void setWindowValue(const QString &key, const QVariant &value);
  Q_INVOKABLE void copyText(const QString &text);

signals:
  void routeChanged();
  void localHistoryChanged();
  void preferencesChanged();
  void pendingRouteChanged();
  void deepLinkReceived(const QString &route);
  void showWindowRequested();
  void backgroundLaunchChanged();
  void foregroundActivated();
  void shareLinkCopied(const QString &url);

private:
  void updateAutoStart(bool enabled);
  void openNotificationLink();

  Database *m_database = nullptr;
  QSettings m_settings;
  QSystemTrayIcon *m_tray = nullptr;
  QMenu *m_trayMenu = nullptr;
  bool m_backgroundLaunch = false;
  bool m_foregroundActivated = true;
  QString m_notificationLink;
  QString m_route = QStringLiteral("home");
  QString m_pendingRoute;
  QVariantList m_localHistory;
};
