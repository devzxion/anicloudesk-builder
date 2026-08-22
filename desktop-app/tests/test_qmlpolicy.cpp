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
    for (const auto *shortcut : {"Space", "Left", "Right", "Up", "Down", "M", "C", "Escape"}) QVERIFY2(playerSource.contains(shortcut), shortcut);
    QVERIFY(playerSource.contains("canSkipIntro"));
    QVERIFY(playerSource.contains("canSkipOutro"));
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
    QVERIFY(mainSource.contains("showBroadcastNotification"));
    QVERIFY(mainSource.contains("onShowWindowRequested"));
  }

  void detailsExposePublicShareLink() {
    QFile details(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/DetailsPage.qml")); QVERIFY(details.open(QIODevice::ReadOnly));
    const auto source = details.readAll();
    QVERIFY(source.contains("text: \"Share\""));
    QVERIFY(source.contains("Runtime.shareAnime"));
  }

  void profileExplainsBackgroundNotifications() {
    QFile profile(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/pages/ProfilePage.qml")); QVERIFY(profile.open(QIODevice::ReadOnly));
    const auto source = profile.readAll();
    QVERIFY(source.contains("Native notifications"));
    QVERIFY(source.contains("Start AniCloud with my computer"));
    QVERIFY(source.contains("Startup is off by default"));
  }

  void runtimeKeepsNotificationsAliveWithoutTheWindow() {
    QFile runtime(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/AppRuntime.cpp")); QVERIFY(runtime.open(QIODevice::ReadOnly));
    QFile main(QStringLiteral(ANICLOUD_SOURCE_DIR "/src/main.cpp")); QVERIFY(main.open(QIODevice::ReadOnly));
    const auto runtimeSource = runtime.readAll();
    const auto mainSource = main.readAll();
    QVERIFY(runtimeSource.contains("updateAutoStart"));
    QVERIFY(runtimeSource.contains("notifications/startAtLogin"));
    QVERIFY(!runtimeSource.contains("updateAutoStart(notificationsEnabled())"));
    QVERIFY(runtimeSource.contains("lastBroadcastId"));
    QVERIFY(mainSource.contains("--background"));
    QVERIFY(mainSource.contains("QLocalServer"));
    QVERIFY(mainSource.contains("60 * 1000"));
  }

  void windowsStartupRequiresExplicitConsent() {
    QFile installer(QStringLiteral(ANICLOUD_SOURCE_DIR "/packaging/windows/installer.nsi")); QVERIFY(installer.open(QIODevice::ReadOnly));
    const auto source = installer.readAll();
    QVERIFY(!source.contains("WriteRegStr HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Run\""));
    QVERIFY(source.contains("DeleteRegValue HKCU \"Software\\Microsoft\\Windows\\CurrentVersion\\Run\""));
  }

  void paletteIsCanonical() {
    QFile theme(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/Theme.qml")); QVERIFY(theme.open(QIODevice::ReadOnly));
    const auto source = theme.readAll(); QVERIFY(source.contains("#E50914")); QVERIFY(source.contains("#B20710")); QVERIFY(source.contains("#09090B"));
  }
};

QTEST_GUILESS_MAIN(QmlPolicyTest)
#include "test_qmlpolicy.moc"
