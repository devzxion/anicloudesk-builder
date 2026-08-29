#include "AppRuntime.h"

#include "Database.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QMenu>
#include <QSaveFile>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QUrl>

namespace {
QString escapedDesktopExec(QString value) {
  value.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  value.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return value;
}
}

AppRuntime::AppRuntime(Database *database, bool backgroundLaunch, QObject *parent)
  : QObject(parent), m_database(database), m_settings(), m_backgroundLaunch(backgroundLaunch) {
  // Every new process is a cold launch and starts at Home. Navigation remains
  // untouched while this process is merely minimized or hidden to the tray.
  m_route = QStringLiteral("home");
  m_foregroundActivated = !backgroundLaunch;
  if (QSystemTrayIcon::isSystemTrayAvailable()) {
    m_tray = new QSystemTrayIcon(QIcon(QStringLiteral(":/qt/qml/AniCloud/resources/icon.png")), this);
    m_tray->setToolTip(QStringLiteral("AniCloud"));
    m_trayMenu = new QMenu;
    auto *openAction = m_trayMenu->addAction(QStringLiteral("Open AniCloud"));
    m_trayMenu->addSeparator();
    auto *quitAction = m_trayMenu->addAction(QStringLiteral("Quit AniCloud"));
    m_tray->setContextMenu(m_trayMenu);
    connect(openAction, &QAction::triggered, this, [this] { activateForeground(); });
    connect(quitAction, &QAction::triggered, this, &AppRuntime::quitApplication);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
      if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) activateForeground();
    });
    connect(m_tray, &QSystemTrayIcon::messageClicked, this, &AppRuntime::openNotificationLink);
    if (notificationsEnabled()) m_tray->show();
    // Recreate a missing registration after an update or install-path change.
    // New installs start quietly by default; an explicit Profile opt-out is
    // persisted and removes the platform registration on every later launch.
    updateAutoStart(startAtLogin());
  }
  refreshLocalHistory();
}

AppRuntime::~AppRuntime() { delete m_trayMenu; }

QString AppRuntime::audioPreference() const { return m_settings.value(QStringLiteral("playback/audio"), QStringLiteral("sub")).toString(); }
QString AppRuntime::playbackQuality() const { return m_settings.value(QStringLiteral("playback/quality"), QStringLiteral("auto")).toString(); }
QString AppRuntime::serverPreference() const { return m_settings.value(QStringLiteral("playback/server"), QStringLiteral("hd-2")).toString(); }
double AppRuntime::captionScale() const { return qBound(0.75, m_settings.value(QStringLiteral("captions/scale"), 1.0).toDouble(), 1.5); }
QString AppRuntime::captionColor() const {
  const QColor color(m_settings.value(QStringLiteral("captions/color"), QStringLiteral("#FFFFFF")).toString());
  return color.isValid() ? color.name(QColor::HexRgb).toUpper() : QStringLiteral("#FFFFFF");
}
double AppRuntime::captionBackgroundOpacity() const {
  return qBound(0.0, m_settings.value(QStringLiteral("captions/backgroundOpacity"), 0.72).toDouble(), 0.9);
}
bool AppRuntime::captionOutline() const { return m_settings.value(QStringLiteral("captions/outline"), true).toBool(); }
int AppRuntime::downloadQuality() const { return m_settings.value(QStringLiteral("downloads/quality"), 1080).toInt(); }
bool AppRuntime::allowMeteredDownloads() const { return m_settings.value(QStringLiteral("downloads/allowMetered"), false).toBool(); }
bool AppRuntime::notificationsEnabled() const { return m_settings.value(QStringLiteral("notifications/enabled"), true).toBool(); }
bool AppRuntime::startAtLogin() const { return m_settings.value(QStringLiteral("notifications/startAtLogin"), true).toBool(); }

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
void AppRuntime::setServerPreference(const QString &value) {
  m_settings.setValue(QStringLiteral("playback/server"), value == QStringLiteral("hd-1") ? QStringLiteral("hd-1") : QStringLiteral("hd-2"));
  emit preferencesChanged();
}
void AppRuntime::setCaptionScale(double value) {
  const auto normalized = qBound(0.75, value, 1.5);
  if (qFuzzyCompare(captionScale(), normalized)) return;
  m_settings.setValue(QStringLiteral("captions/scale"), normalized); emit preferencesChanged();
}
void AppRuntime::setCaptionColor(const QString &value) {
  const QColor color(value);
  if (!color.isValid()) return;
  const auto normalized = color.name(QColor::HexRgb).toUpper();
  if (captionColor() == normalized) return;
  m_settings.setValue(QStringLiteral("captions/color"), normalized); emit preferencesChanged();
}
void AppRuntime::setCaptionBackgroundOpacity(double value) {
  const auto normalized = qBound(0.0, value, 0.9);
  if (qFuzzyCompare(captionBackgroundOpacity(), normalized)) return;
  m_settings.setValue(QStringLiteral("captions/backgroundOpacity"), normalized); emit preferencesChanged();
}
void AppRuntime::setCaptionOutline(bool value) {
  if (captionOutline() == value) return;
  m_settings.setValue(QStringLiteral("captions/outline"), value); emit preferencesChanged();
}
void AppRuntime::setDownloadQuality(int value) { m_settings.setValue(QStringLiteral("downloads/quality"), value); emit preferencesChanged(); }
void AppRuntime::setAllowMeteredDownloads(bool value) { m_settings.setValue(QStringLiteral("downloads/allowMetered"), value); emit preferencesChanged(); }
void AppRuntime::setNotificationsEnabled(bool value) {
  if (notificationsEnabled() == value) return;
  m_settings.setValue(QStringLiteral("notifications/enabled"), value);
  m_settings.sync();
  if (!value && startAtLogin()) setStartAtLogin(false);
  if (m_tray) value ? m_tray->show() : m_tray->hide();
  emit preferencesChanged();
}

void AppRuntime::setStartAtLogin(bool value) {
  value = value && notificationsEnabled() && m_tray;
  if (startAtLogin() == value) return;
  m_settings.setValue(QStringLiteral("notifications/startAtLogin"), value);
  m_settings.sync();
  updateAutoStart(value);
  emit preferencesChanged();
}

void AppRuntime::refreshLocalHistory() { m_localHistory = m_database->localHistory(200); emit localHistoryChanged(); }
void AppRuntime::deleteLocalHistory(const QString &animeId, const QString &episodeId) {
  if (m_database->removeLocalHistory(animeId, episodeId)) refreshLocalHistory();
}

void AppRuntime::handleDeepLink(const QString &value) {
  const QUrl url(value);
  QString route;
  if (url.scheme() == QStringLiteral("anicloud")) {
    route = url.host();
    if (!url.path().isEmpty() && url.path() != QStringLiteral("/")) route += url.path();
  } else if (url.scheme() == QStringLiteral("https") && url.host().compare(QStringLiteral("anicloud.ink"), Qt::CaseInsensitive) == 0) {
    const auto parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.size() == 2 && parts.first() == QStringLiteral("anime")) route = QStringLiteral("details/") + parts.at(1);
  }
  if (route.isEmpty() && !value.startsWith(QStringLiteral("anicloud:"), Qt::CaseInsensitive)) return;
  if (route.isEmpty()) route = QStringLiteral("home");
  setRoute(route);
  emit deepLinkReceived(route);
  activateForeground(false);
}

void AppRuntime::restorePendingRoute() {
  if (m_pendingRoute.isEmpty()) return; setRoute(m_pendingRoute); m_pendingRoute.clear(); emit pendingRouteChanged();
}

void AppRuntime::showNotification(const QString &title, const QString &message) {
  if (!notificationsEnabled() || !m_tray) return;
  m_notificationLink.clear();
  m_tray->showMessage(title, message, QSystemTrayIcon::Information, 8000);
}

void AppRuntime::showBroadcastNotification(const QString &id, const QString &title,
                                           const QString &message, const QString &linkUrl) {
  if (id.isEmpty() || !notificationsEnabled() || !m_tray) return;
  const auto lastId = m_settings.value(QStringLiteral("notifications/lastBroadcastId")).toString();
  if (lastId == id) return;
  m_settings.setValue(QStringLiteral("notifications/lastBroadcastId"), id);
  m_settings.sync();
  m_notificationLink = linkUrl;
  m_tray->showMessage(title.isEmpty() ? QStringLiteral("AniCloud") : title,
                      message, QSystemTrayIcon::Information, 10000);
}

QString AppRuntime::animeShareUrl(const QString &animeId) const {
  const auto trimmed = animeId.trimmed();
  if (trimmed.isEmpty()) return {};
  return QStringLiteral("https://anicloud.ink/anime/") + QString::fromLatin1(QUrl::toPercentEncoding(trimmed));
}

void AppRuntime::shareAnime(const QString &animeId, const QString &) {
  const auto url = animeShareUrl(animeId);
  if (url.isEmpty()) return;
  QApplication::clipboard()->setText(url);
  emit shareLinkCopied(url);
}

void AppRuntime::requestShow() { emit showWindowRequested(); }

void AppRuntime::activateForeground(bool resetColdRoute) {
  const bool firstForeground = !m_foregroundActivated;
  m_foregroundActivated = true;
  if (m_backgroundLaunch) {
    m_backgroundLaunch = false;
    emit backgroundLaunchChanged();
  }
  if (firstForeground && resetColdRoute) setRoute(QStringLiteral("home"));
  if (firstForeground) emit foregroundActivated();
  requestShow();
}

void AppRuntime::quitApplication() { QCoreApplication::quit(); }

void AppRuntime::openNotificationLink() {
  if (m_notificationLink.isEmpty()) {
    activateForeground();
    return;
  }
  const QUrl url(m_notificationLink);
  if (url.scheme() == QStringLiteral("anicloud") ||
      (url.scheme() == QStringLiteral("https") && url.host().compare(QStringLiteral("anicloud.ink"), Qt::CaseInsensitive) == 0 && url.path().startsWith(QStringLiteral("/anime/")))) {
    handleDeepLink(m_notificationLink);
  } else if (url.isValid() && (url.scheme() == QStringLiteral("https") || url.scheme() == QStringLiteral("http"))) {
    QDesktopServices::openUrl(url);
  }
}

void AppRuntime::updateAutoStart(bool enabled) {
  enabled = enabled && m_tray;
  auto executable = QCoreApplication::applicationFilePath();
#if defined(Q_OS_LINUX)
  const auto appImage = qEnvironmentVariable("APPIMAGE");
  if (!appImage.isEmpty()) executable = appImage;
#endif
#if defined(Q_OS_WIN)
  QSettings runKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                   QSettings::NativeFormat);
  if (enabled) runKey.setValue(QStringLiteral("AniCloud"), QStringLiteral("\"") + QDir::toNativeSeparators(executable) + QStringLiteral("\" --background"));
  else runKey.remove(QStringLiteral("AniCloud"));
  runKey.sync();
#elif defined(Q_OS_MACOS)
  const auto path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
                    + QStringLiteral("/Library/LaunchAgents/ink.anicloud.desktop.notifications.plist");
  if (!enabled) { QFile::remove(path); return; }
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  const auto escaped = executable.toHtmlEscaped();
  file.write(QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                            "<plist version=\"1.0\"><dict>"
                            "<key>Label</key><string>ink.anicloud.desktop.notifications</string>"
                            "<key>ProgramArguments</key><array><string>%1</string><string>--background</string></array>"
                            "<key>RunAtLoad</key><true/>"
                            "<key>ProcessType</key><string>Interactive</string>"
                            "</dict></plist>\n").arg(escaped).toUtf8());
  file.commit();
#else
  const auto path = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                    + QStringLiteral("/autostart/ink.anicloud.desktop.notifications.desktop");
  if (!enabled) { QFile::remove(path); return; }
  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) return;
  file.write(QStringLiteral("[Desktop Entry]\nType=Application\nName=AniCloud background notifications\n"
                            "Comment=Receive AniCloud notifications after sign-in\n"
                            "Exec=\"%1\" --background\nTerminal=false\nX-GNOME-Autostart-enabled=true\n")
               .arg(escapedDesktopExec(executable)).toUtf8());
  file.commit();
#endif
}

QVariant AppRuntime::windowValue(const QString &key, const QVariant &fallback) const { return m_settings.value(QStringLiteral("window/") + key, fallback); }
void AppRuntime::setWindowValue(const QString &key, const QVariant &value) { m_settings.setValue(QStringLiteral("window/") + key, value); }
void AppRuntime::copyText(const QString &text) { QApplication::clipboard()->setText(text); }
