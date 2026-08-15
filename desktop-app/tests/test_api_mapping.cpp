#include "ApiClient.h"

#include <QJsonArray>
#include <QTest>

class ApiMappingTest final : public QObject {
  Q_OBJECT
private slots:
  void mapsLegacyAndCurrentCards() {
    const QJsonArray source{
      QJsonObject{{QStringLiteral("id"), QStringLiteral("one")}, {QStringLiteral("name"), QStringLiteral("First")}, {QStringLiteral("image"), QStringLiteral("poster")}, {QStringLiteral("episodes"), QJsonObject{{QStringLiteral("sub"), 12}, {QStringLiteral("dub"), 4}}}},
      QJsonObject{{QStringLiteral("animeId"), QStringLiteral("two")}, {QStringLiteral("title"), QStringLiteral("Second")}, {QStringLiteral("poster"), QStringLiteral("poster-2")}},
    };
    const auto mapped = ProviderClient::cardList(source);
    QCOMPARE(mapped.size(), 2);
    QCOMPARE(mapped.at(0).toMap().value(QStringLiteral("title")).toString(), QStringLiteral("First"));
    QCOMPARE(mapped.at(0).toMap().value(QStringLiteral("subEpisodes")).toInt(), 12);
    QCOMPARE(mapped.at(1).toMap().value(QStringLiteral("id")).toString(), QStringLiteral("two"));
  }

  void mapsPagedEpisodes() {
    const QJsonObject source{{QStringLiteral("items"), QJsonArray{
      QJsonObject{{QStringLiteral("episodeId"), QStringLiteral("ep-13")}, {QStringLiteral("episodeNumber"), 13}, {QStringLiteral("name"), QStringLiteral("Return")}},
    }}};
    const auto mapped = ProviderClient::episodeList(source, QStringLiteral("anime"));
    QCOMPARE(mapped.size(), 1);
    QCOMPARE(mapped.first().toMap().value(QStringLiteral("animeId")).toString(), QStringLiteral("anime"));
    QCOMPARE(mapped.first().toMap().value(QStringLiteral("number")).toInt(), 13);

    const auto legacy = ProviderClient::episodeList(QJsonArray{QJsonArray{14, QStringLiteral("anime-episode-14")}}, QStringLiteral("anime"));
    QCOMPARE(legacy.size(), 1);
    QCOMPARE(legacy.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("anime-episode-14"));
    QCOMPARE(legacy.first().toMap().value(QStringLiteral("number")).toInt(), 14);
  }

  void mapsStreamHeadersFallbacksCaptionsAndSkips() {
    const QJsonObject root{{QStringLiteral("results"), QJsonObject{{QStringLiteral("stream"), QJsonObject{
      {QStringLiteral("Referer"), QStringLiteral("https://embed.example/watch")},
      {QStringLiteral("sources"), QJsonArray{
        QJsonObject{{QStringLiteral("file"), QStringLiteral("https://cdn.example/720.m3u8")}, {QStringLiteral("label"), QStringLiteral("720p")}},
        QJsonObject{{QStringLiteral("file"), QStringLiteral("https://cdn.example/master.m3u8")}, {QStringLiteral("label"), QStringLiteral("Auto")}},
      }},
      {QStringLiteral("tracks"), QJsonArray{QJsonObject{{QStringLiteral("file"), QStringLiteral("https://cdn.example/en.vtt")}, {QStringLiteral("kind"), QStringLiteral("captions")}, {QStringLiteral("label"), QStringLiteral("English")}}}},
      {QStringLiteral("intro"), QJsonObject{{QStringLiteral("start"), 4}, {QStringLiteral("end"), 89}}},
      {QStringLiteral("outro"), QJsonObject{{QStringLiteral("start"), 1300}, {QStringLiteral("end"), 1360}}},
    }}}}};
    const auto stream = ProviderClient::streamMap(root, QStringLiteral("episode"), QStringLiteral("hd-1"), QStringLiteral("sub"));
    QCOMPARE(stream.value(QStringLiteral("mediaUrl")).toString(), QStringLiteral("https://cdn.example/master.m3u8"));
    QCOMPARE(stream.value(QStringLiteral("alternates")).toList().size(), 2);
    QCOMPARE(stream.value(QStringLiteral("subtitles")).toList().size(), 1);
    QCOMPARE(stream.value(QStringLiteral("introEnd")).toInt(), 89);
    QCOMPARE(stream.value(QStringLiteral("headers")).toMap().value(QStringLiteral("Referer")).toString(), QStringLiteral("https://embed.example/watch"));
    QCOMPARE(stream.value(QStringLiteral("headers")).toMap().value(QStringLiteral("Origin")).toString(), QStringLiteral("https://embed.example"));
  }
};

QTEST_GUILESS_MAIN(ApiMappingTest)
#include "test_api_mapping.moc"
