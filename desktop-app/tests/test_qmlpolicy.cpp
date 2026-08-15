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
  }

  void paletteIsCanonical() {
    QFile theme(QStringLiteral(ANICLOUD_SOURCE_DIR "/qml/Theme.qml")); QVERIFY(theme.open(QIODevice::ReadOnly));
    const auto source = theme.readAll(); QVERIFY(source.contains("#E50914")); QVERIFY(source.contains("#B20710")); QVERIFY(source.contains("#09090B"));
  }
};

QTEST_GUILESS_MAIN(QmlPolicyTest)
#include "test_qmlpolicy.moc"
