#include <QFile>
#include <QTest>

class QmlPolicyTest final : public QObject {
  Q_OBJECT
private slots:
  void allRoutesAreCompiled() {
    QFile cmake(QStringLiteral(ANICLOUD_SOURCE_DIR "/CMakeLists.txt")); QVERIFY(cmake.open(QIODevice::ReadOnly));
    const auto source = cmake.readAll();
    for (const auto *page : {"HomePage.qml", "DiscoverPage.qml", "DetailsPage.qml", "DownloadsPage.qml", "LibraryPage.qml", "ProfilePage.qml", "AuthPage.qml", "BroadcastsPage.qml", "PlayerPage.qml"})
      QVERIFY2(source.contains(page), page);
  }

  void interactiveShellHasAccessibilityMetadata() {
    QFile navigation(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/components/NavigationRail.qml")); QVERIFY(navigation.open(QIODevice::ReadOnly));
    QFile player(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/PlayerPage.qml")); QVERIFY(player.open(QIODevice::ReadOnly));
    QVERIFY(navigation.readAll().contains("Accessible.name"));
    const auto playerSource = player.readAll();
    for (const auto *shortcut : {"Space", "Left", "Right", "Up", "Down", "M", "C", "N", "Escape"}) QVERIFY2(playerSource.contains(shortcut), shortcut);
    QVERIFY(playerSource.contains("canSkipIntro"));
    QVERIFY(playerSource.contains("canSkipOutro"));
    QVERIFY(playerSource.contains("interval: 4000"));
    QVERIFY(playerSource.contains("videoOutput.videoSink.subtitleText"));
    QVERIFY(playerSource.contains("Player.subtitleText"));
    QVERIFY(playerSource.contains("Player.captionStatus"));
    QVERIFY(playerSource.contains("Player.toggleCaptions()"));
    QVERIFY(playerSource.contains("function chooseCaption(trackIndex)"));
    QVERIFY(playerSource.contains("required property int index"));
    QVERIFY(playerSource.contains("onClicked: root.chooseCaption(index)"));
    QVERIFY(playerSource.contains("Caption appearance"));
    QVERIFY(playerSource.contains("Runtime.captionScale"));
    QVERIFY(playerSource.contains("Runtime.captionColor"));
    QVERIFY(playerSource.contains("Runtime.captionBackgroundOpacity"));
    QVERIFY(playerSource.contains("uiLocked"));
    QVERIFY(playerSource.contains("onDoubleClicked: root.unlockPlayerUi()"));
    QVERIFY(playerSource.contains("Double-click the lock to unlock"));
  }

  void shellBrandingAndNavigationMatchDesktopDesign() {
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/Main.qml")); QVERIFY(main.open(QIODevice::ReadOnly));
    QFile navigation(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/components/NavigationRail.qml")); QVERIFY(navigation.open(QIODevice::ReadOnly));
    const auto mainSource = main.readAll();
    const auto navigationSource = navigation.readAll();
    QVERIFY(mainSource.contains("text: \"Ani\""));
    QVERIFY(mainSource.contains("text: \"Cloud\""));
    QVERIFY(!mainSource.contains("text: \"NATIVE\""));
    QVERIFY(!navigationSource.contains("text: \"ANICLOUD\""));
    QVERIFY(navigationSource.contains("hover.hovered"));
    QVERIFY(navigationSource.contains("navigationFocus.activeFocus"));
    QVERIFY(navigationSource.contains("routeSelected"));
    QVERIFY(navigationSource.contains("Notifications"));
    QVERIFY(mainSource.contains("onShowWindowRequested"));
    QVERIFY(mainSource.contains("WindowControlButton"));
  }

  void detailsExposePublicShareLink() {
    QFile details(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/DetailsPage.qml")); QVERIFY(details.open(QIODevice::ReadOnly));
    const auto source = details.readAll();
    QVERIFY(source.contains("text: \"Share\""));
    QVERIFY(source.contains("Runtime.shareAnime"));
    QVERIFY(source.contains("Episode number or title"));
    QVERIFY(source.contains("episodeName"));
  }

  void profileExplainsBackgroundNotifications() {
    QFile profile(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/ProfilePage.qml")); QVERIFY(profile.open(QIODevice::ReadOnly));
    const auto source = profile.readAll();
    QVERIFY(source.contains("Native notifications"));
    QVERIFY(source.contains("Start AniCloud with my computer"));
    QVERIFY(source.contains("starts quietly with your computer"));
  }

  void runtimeKeepsNotificationsAliveWithoutTheWindow() {
    QFile runtime(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/AppRuntime.cpp")); QVERIFY(runtime.open(QIODevice::ReadOnly));
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/main.cpp")); QVERIFY(main.open(QIODevice::ReadOnly));
    const auto runtimeSource = runtime.readAll();
    const auto mainSource = main.readAll();
    QVERIFY(runtimeSource.contains("updateAutoStart"));
    QVERIFY(runtimeSource.contains("notifications/startAtLogin"));
    QVERIFY(runtimeSource.contains("notifications/startAtLogin\"), true"));
    QVERIFY(runtimeSource.contains("updateAutoStart(startAtLogin())"));
    QVERIFY(runtimeSource.contains("lastBroadcastId"));
    QVERIFY(mainSource.contains("--background"));
    QVERIFY(mainSource.contains("QLocalServer"));
    QVERIFY(mainSource.contains("60 * 1000"));
    QVERIFY(mainSource.contains("AccountClient::broadcastsChanged"));
    QVERIFY(mainSource.contains("showBroadcastNotification"));
    QVERIFY(mainSource.contains("else QTimer::singleShot(3000, &updates, &UpdateService::check)"));
    QVERIFY(mainSource.contains("AniCloud update available"));
  }

  void backgroundActivationLoadsHomeAndColdLaunchDoesNotRestoreTheLastRoute() {
    QFile runtime(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/AppRuntime.cpp")); QVERIFY(runtime.open(QIODevice::ReadOnly));
    QFile runtimeHeader(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/AppRuntime.h")); QVERIFY(runtimeHeader.open(QIODevice::ReadOnly));
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/main.cpp")); QVERIFY(main.open(QIODevice::ReadOnly));
    const auto runtimeSource = runtime.readAll();
    const auto headerSource = runtimeHeader.readAll();
    const auto mainSource = main.readAll();
    QVERIFY(runtimeSource.contains("m_route = QStringLiteral(\"home\")"));
    QVERIFY(!runtimeSource.contains("navigation/lastRoute\"), QStringLiteral(\"home\")"));
    QVERIFY(runtimeSource.contains("activateForeground"));
    QVERIFY(headerSource.contains("backgroundLaunchChanged"));
    QVERIFY(headerSource.contains("foregroundActivated"));
    QVERIFY(mainSource.contains("AppRuntime::foregroundActivated"));
    QVERIFY(mainSource.contains("ProviderClient::loadHome"));
    QVERIFY(mainSource.contains("runtime.activateForeground()"));
  }

  void pagesSupportScrollUpRefresh() {
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/Main.qml")); QVERIFY(main.open(QIODevice::ReadOnly));
    const auto mainSource = main.readAll();
    QVERIFY(mainSource.contains("WheelHandler"));
    QVERIFY(mainSource.contains("refreshWheelDistance >= 360"));
    QVERIFY(mainSource.contains("Scroll up to refresh"));
    for (const auto *page : {"HomePage.qml", "DiscoverPage.qml", "DetailsPage.qml", "DownloadsPage.qml",
                             "LibraryPage.qml", "ProfilePage.qml", "BroadcastsPage.qml"}) {
      QFile source(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/") + QString::fromLatin1(page));
      QVERIFY2(source.open(QIODevice::ReadOnly), page);
      const auto contents = source.readAll();
      QVERIFY2(contents.contains("atRefreshBoundary"), page);
      QVERIFY2(contents.contains("refreshPage"), page);
    }
  }

  void windowAndPlayerControlsUseCenteredSvgIcons() {
    QFile cmake(QStringLiteral(ANICLOUD_SOURCE_DIR "/CMakeLists.txt")); QVERIFY(cmake.open(QIODevice::ReadOnly));
    QFile player(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/PlayerPage.qml")); QVERIFY(player.open(QIODevice::ReadOnly));
    const auto cmakeSource = cmake.readAll();
    for (const auto *asset : {"WindowControlButton.qml", "window-minimize.svg", "window-maximize.svg",
                              "window-restore.svg", "window-close.svg"})
      QVERIFY2(cmakeSource.contains(asset), asset);
    const auto playerSource = player.readAll();
    QVERIFY(playerSource.contains("implicitWidth: 136"));
    QVERIFY(playerSource.contains("sourceSize.width: 18"));
  }

  void windowsStartupChoiceSurvivesInstallerUpdates() {
    QFile installer(QStringLiteral(ANICLOUD_SOURCE_DIR "/packaging/windows/installer.nsi")); QVERIFY(installer.open(QIODevice::ReadOnly));
    const auto source = installer.readAll();
    QVERIFY(!source.contains("WriteRegStr HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Run\""));
    QVERIFY(source.contains("DeleteRegValue HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Run\""));
    QVERIFY(source.contains("Preserve the user's startup choice across updates"));
  }

  void updateInstallationClosesBackgroundProcessAndCanRetry() {
    QFile installer(QStringLiteral(ANICLOUD_SOURCE_DIR "/packaging/windows/installer.nsi")); QVERIFY(installer.open(QIODevice::ReadOnly));
    QFile update(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/UpdateService.cpp")); QVERIFY(update.open(QIODevice::ReadOnly));
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/main.cpp")); QVERIFY(main.open(QIODevice::ReadOnly));
    const auto installerSource = installer.readAll();
    QVERIFY(installerSource.contains("taskkill.exe"));
    QVERIFY(installerSource.contains("tasklist.exe"));
    QVERIFY(installerSource.contains("MB_RETRYCANCEL"));
    QVERIFY(!installerSource.contains("/T /F"));
    QVERIFY(update.readAll().contains("installerStarted"));
    const auto mainSource = main.readAll();
    QVERIFY(mainSource.contains("UpdateService::installerStarted"));
    QVERIFY(mainSource.contains("QCoreApplication::quit"));
    QVERIFY(mainSource.contains("--update-installer-helper"));
    QVERIFY(mainSource.contains("runInstallerHelper"));
  }

  void downloadsPersistTheResolvedEpisodeIdentity() {
    QFile downloads(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/DownloadManager.cpp")); QVERIFY(downloads.open(QIODevice::ReadOnly));
    QFile player(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/PlayerController.cpp")); QVERIFY(player.open(QIODevice::ReadOnly));
    QFile gateway(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/HlsGateway.cpp")); QVERIFY(gateway.open(QIODevice::ReadOnly));
    const auto source = downloads.readAll();
    QVERIFY(source.contains("record.insert(QStringLiteral(\"episodeId\")"));
    QVERIFY(source.contains("record.insert(QStringLiteral(\"audioMode\")"));
    QVERIFY(source.contains("record.insert(QStringLiteral(\"server\")"));
    QVERIFY(source.contains("ConcurrentResources = 4"));
    QVERIFY(source.contains("DownloadRetryPolicy::shouldRetry"));
    QVERIFY(source.contains("Retry-After"));
    QVERIFY(source.contains("pendingRetries"));
    QVERIFY(source.contains("m_groups"));
    QVERIFY(source.contains("episodeStatus"));
    QVERIFY(source.contains("offlineExtension(url, resourceKind)"));
    const auto playerSource = player.readAll();
    QVERIFY(playerSource.contains("openOfflineSession"));
    QVERIFY(playerSource.contains("m_offlinePlayback"));
    QVERIFY(!playerSource.contains("m_player.setSource(QUrl::fromLocalFile(path))"));
    const auto gatewaySource = gateway.readAll();
    QVERIFY(gatewaySource.contains("serveLocal"));
    QVERIFY(gatewaySource.contains("pathIsWithin"));
  }

  void downloadAndDiscoverPagesExposeNativeOrganization() {
    QFile downloads(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/DownloadsPage.qml")); QVERIFY(downloads.open(QIODevice::ReadOnly));
    QFile details(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/DetailsPage.qml")); QVERIFY(details.open(QIODevice::ReadOnly));
    QFile discover(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/DiscoverPage.qml")); QVERIFY(discover.open(QIODevice::ReadOnly));
    QFile card(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/components/AnimeCard.qml")); QVERIFY(card.open(QIODevice::ReadOnly));
    const auto downloadSource = downloads.readAll();
    const auto detailsSource = details.readAll();
    const auto discoverSource = discover.readAll();
    QVERIFY(downloadSource.contains("Downloads.groups"));
    QVERIFY(downloadSource.contains("View episodes"));
    QVERIFY(detailsSource.contains("Downloads.episodeStatus"));
    QVERIFY(detailsSource.contains("Play offline"));
    QVERIFY(discoverSource.contains("Provider.genres"));
    QVERIFY(discoverSource.contains("Provider.themes"));
    QVERIFY(discoverSource.contains("Provider.browseGenre"));
    QVERIFY(card.readAll().contains("animeImage"));
  }

  void hd2IsTheDesktopDefaultServer() {
    QFile runtime(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/AppRuntime.cpp")); QVERIFY(runtime.open(QIODevice::ReadOnly));
    QFile player(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/PlayerController.h")); QVERIFY(player.open(QIODevice::ReadOnly));
    const auto runtimeSource = runtime.readAll();
    QVERIFY(runtimeSource.contains("playback/server"));
    QVERIFY(runtimeSource.contains("QStringLiteral(\"hd-2\")"));
    QVERIFY(player.readAll().contains("m_server = QStringLiteral(\"hd-2\")"));
  }

  void paletteIsCanonical() {
    QFile theme(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/Theme.qml")); QVERIFY(theme.open(QIODevice::ReadOnly));
    const auto source = theme.readAll(); QVERIFY(source.contains("#E50914")); QVERIFY(source.contains("#B20710")); QVERIFY(source.contains("#09090B"));
  }
};

QTEST_GUILESS_MAIN(QmlPolicyTest)
#include "test_qmlpolicy.moc"
