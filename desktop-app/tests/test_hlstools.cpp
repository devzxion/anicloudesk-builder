#include "HlsTools.h"

#include <QFile>
#include <QTest>

class HlsToolsTest final : public QObject {
  Q_OBJECT
private slots:
  void selectsPreferredVariant() {
    QFile fixture(QStringLiteral(ANICLOUD_FIXTURE_DIR "/master.m3u8")); QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto items = HlsTools::variants(fixture.readAll(), QUrl(QStringLiteral("https://media.example/master.m3u8")));
    QCOMPARE(items.size(), 2);
    QCOMPARE(HlsTools::selectVariant(items, 720).height, 480);
    QCOMPARE(HlsTools::selectVariant(items, 1080).height, 1080);
  }

  void enumeratesKeysMapsRangesAndSegments() {
    QFile fixture(QStringLiteral(ANICLOUD_FIXTURE_DIR "/media.m3u8")); QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto items = HlsTools::resources(fixture.readAll(), QUrl(QStringLiteral("https://media.example/path/index.m3u8")));
    QCOMPARE(items.size(), 4);
    QCOMPARE(items.at(0).url, QUrl(QStringLiteral("https://media.example/path/keys/key.bin")));
    QCOMPARE(items.at(1).kind, QStringLiteral("map"));
    QCOMPARE(items.at(3).byteRange, QStringLiteral("900@720"));
  }

  void rewritesOnlyDiscoveredUrls() {
    QFile fixture(QStringLiteral(ANICLOUD_FIXTURE_DIR "/media.m3u8")); QVERIFY(fixture.open(QIODevice::ReadOnly));
    const auto output = HlsTools::rewrite(fixture.readAll(), QUrl(QStringLiteral("https://media.example/path/index.m3u8")),
      [](const QUrl &url) { return QUrl(QStringLiteral("http://127.0.0.1:1234/opaque/") + QString::number(qHash(url))); });
    QVERIFY(output.contains("127.0.0.1:1234/opaque"));
    QVERIFY(!output.contains("media.example"));
    QVERIFY(output.startsWith("#EXTM3U"));
  }

  void attachesOnlineAndOfflineCaptionTracks() {
    const QByteArray master("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000000\nvideo.m3u8");
    const QList<QPair<QString, QUrl>> captions{
      {QStringLiteral("English"), QUrl(QStringLiteral("http://127.0.0.1/subtitle"))},
    };
    const auto online = HlsTools::addSubtitleTracks(master, captions);
    QVERIFY(online.contains("#EXT-X-MEDIA:TYPE=SUBTITLES"));
    QVERIFY(online.contains("SUBTITLES=\"anicloud-subs\""));
    QVERIFY(online.contains("http://127.0.0.1/subtitle"));

    const auto offline = HlsTools::makeOfflineMaster(QUrl(QStringLiteral("media.m3u8")), {
      {QStringLiteral("English"), QUrl(QStringLiteral("resources/en.vtt"))},
    });
    QVERIFY(offline.contains("resources/en.vtt"));
    QVERIFY(offline.endsWith("media.m3u8"));
  }
};

QTEST_GUILESS_MAIN(HlsToolsTest)
#include "test_hlstools.moc"
