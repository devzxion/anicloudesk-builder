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

    const QJsonArray legacyPayload{QJsonValue(QJsonArray{14, QStringLiteral("anime-episode-14")})};
    const auto legacy = ProviderClient::episodeList(legacyPayload, QStringLiteral("anime"));
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
      {QStringLiteral("tracks"), QJsonArray{
        QJsonObject{{QStringLiteral("file"), QStringLiteral("https://cdn.example/en.vtt")}, {QStringLiteral("kind"), QStringLiteral("captions")}, {QStringLiteral("label"), QStringLiteral("English")}},
        QJsonObject{{QStringLiteral("kind"), QStringLiteral("captions")}, {QStringLiteral("label"), QStringLiteral("Broken")}},
      }},
      {QStringLiteral("intro"), QJsonObject{{QStringLiteral("start"), 4}, {QStringLiteral("end"), 89}}},
      {QStringLiteral("outro"), QJsonObject{{QStringLiteral("start"), 1300}, {QStringLiteral("end"), 1360}}},
    }}}}};
    const auto stream = ProviderClient::streamMap(root, QStringLiteral("episode"), QStringLiteral("hd-1"), QStringLiteral("sub"));
    QCOMPARE(stream.value(QStringLiteral("mediaUrl")).toString(), QStringLiteral("https://cdn.example/master.m3u8"));
    QCOMPARE(stream.value(QStringLiteral("alternates")).toList().size(), 2);
    QCOMPARE(stream.value(QStringLiteral("subtitles")).toList().size(), 1);
    QCOMPARE(stream.value(QStringLiteral("introEnd")).toInt(), 89);
    QCOMPARE(stream.value(QStringLiteral("headers")).toMap().value(QStringLiteral("Referer")).toString(), QStringLiteral("https://embed.example/watch"));
    QVERIFY(stream.value(QStringLiteral("headers")).toMap().value(QStringLiteral("Origin")).toString().isEmpty());
    QVERIFY(stream.value(QStringLiteral("headers")).toMap().value(QStringLiteral("User-Agent")).toString().startsWith(QStringLiteral("Mozilla/")));
  }

  void parsesBundledCatalogPagesWithoutAccountApi() {
    const auto topHtml = QStringLiteral(
      "<tr class=\"ranking-list\">"
      "<td><a href=\"https://myanimelist.net/anime/20/Naruto\"><img data-src=\"https://cdn.myanimelist.net/r/50x70/images/anime/naruto.jpg\"></a></td>"
      "<td><h3 class=\"anime_ranking_h3\"><a href=\"https://myanimelist.net/anime/20/Naruto\">Naruto</a></h3>"
      "<div class=\"information\">TV (220 eps)<br>23 min</div></td>"
      "<td><span class=\"top-anime-rank-text\">1</span></td></tr>");
    const auto top = ProviderClient::parseTopAnimeHtml(topHtml, 10);
    QCOMPARE(top.size(), 1);
    QCOMPARE(top.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("20"));
    QCOMPARE(top.first().toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Naruto"));
    QCOMPARE(top.first().toMap().value(QStringLiteral("episodes")).toInt(), 220);
    QVERIFY(top.first().toMap().value(QStringLiteral("poster")).toString().contains(QStringLiteral("/images/anime/naruto.jpg")));

    const auto searchHtml = QStringLiteral(
      "<tr><td><a class=\"hoverinfo_trigger\" href=\"https://myanimelist.net/anime/1735/Naruto__Shippuuden\"><img data-src=\"https://cdn.example/shippuden.jpg\"></a></td>"
      "<td><a class=\"hoverinfo_trigger\" href=\"https://myanimelist.net/anime/1735/Naruto__Shippuuden\"><strong>Naruto: Shippuuden</strong></a></td>"
      "<td>TV</td><td>500</td><td>8.29</td></tr>");
    const auto search = ProviderClient::parseSearchHtml(searchHtml, 20);
    QCOMPARE(search.size(), 1);
    QCOMPARE(search.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("1735"));
    QCOMPARE(search.first().toMap().value(QStringLiteral("episodes")).toInt(), 500);
  }

  void parsesBundledDetailsAndRecommendations() {
    const auto html = QStringLiteral(
      "<h1 class=\"title-name h1_bold_none\"><strong>Naruto</strong></h1>"
      "<p class=\"title-english\">Naruto</p>"
      "<img itemprop=\"image\" data-src=\"https://cdn.example/naruto.jpg\">"
      "<p itemprop=\"description\">A young ninja searches for recognition.</p>"
      "<div class=\"spaceit_pad\"><span class=\"dark_text\">Type:</span> TV</div>"
      "<div class=\"spaceit_pad\"><span class=\"dark_text\">Episodes:</span> 220</div>"
      "<div class=\"spaceit_pad\"><span class=\"dark_text\">Status:</span> Finished Airing</div>"
      "<section id=\"anime_recommendation\"><a href=\"https://myanimelist.net/anime/1735/Naruto__Shippuuden\">Naruto: Shippuuden</a></section>");
    QVariantList recommendations;
    const auto details = ProviderClient::parseAnimeDetailsHtml(html, QStringLiteral("20"), &recommendations);
    QCOMPARE(details.value(QStringLiteral("title")).toString(), QStringLiteral("Naruto"));
    QCOMPARE(details.value(QStringLiteral("episodes")).toInt(), 220);
    QCOMPARE(details.value(QStringLiteral("status")).toString(), QStringLiteral("Finished Airing"));
    QCOMPARE(recommendations.size(), 1);
    QCOMPARE(recommendations.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("1735"));
  }

  void parsesRealEpisodeNames() {
    const auto html = QStringLiteral(
      "<tr class=\"episode-list-data\">"
      "<td class=\"episode-number nowrap\" data-raw=\"1\">1</td>"
      "<td class=\"episode-title fs12\"><a href=\"/anime/20/Naruto/episode/1\">Enter: Naruto Uzumaki!</a>"
      "<br><span>Sanjou! Uzumaki Naruto (参上！うずまきナルト)</span></td></tr>");
    const auto episodes = ProviderClient::parseEpisodeNamesHtml(html, QStringLiteral("20"));
    QCOMPARE(episodes.size(), 1);
    QCOMPARE(episodes.first().toMap().value(QStringLiteral("number")).toInt(), 1);
    QCOMPARE(episodes.first().toMap().value(QStringLiteral("title")).toString(), QStringLiteral("Enter: Naruto Uzumaki!"));
    QCOMPARE(episodes.first().toMap().value(QStringLiteral("id")).toString(), QStringLiteral("20::ep=1"));
  }

  void infersLongRunningEpisodeCountFromListingPagination() {
    const auto html = QStringLiteral(
      "<div class=\"pagination ac\">"
      "<a class=\"link current\" href=\"/anime/21/One_Piece/episode?offset=0\">1 - 100</a>"
      "<a class=\"link\" href=\"/anime/21/One_Piece/episode?offset=100\">101 - 200</a>"
      "<span class=\"skip\">&gt;</span>"
      "<a class=\"link\" href=\"/anime/21/One_Piece/episode?offset=1100\">1101 - 1174</a>"
      "</div>"
      "<tr class=\"episode-list-data\"><td class=\"episode-number\" data-raw=\"100\">100</td>"
      "<td class=\"episode-title\"><a href=\"/anime/21/One_Piece/episode/100\">Episode title</a></td></tr>");
    QCOMPARE(ProviderClient::parseEpisodeCountHtml(html), 1174);
    QCOMPARE(ProviderClient::parseEpisodeCountHtml(html, 1200), 1200);
  }

  void infersEpisodeCountWithoutPagination() {
    const auto html = QStringLiteral(
      "<tr class=\"episode-list-data\"><td class=\"episode-number nowrap\" data-raw=\"7\">7</td>"
      "<td class=\"episode-title\"><a href=\"/anime/1/Show/episode/7\">Finale</a></td></tr>");
    QCOMPARE(ProviderClient::parseEpisodeCountHtml(html), 7);
  }

  void discoversCanonicalSluggedEpisodeListing() {
    const auto html = QStringLiteral(
      "<a href=\"https://attacker.example/anime/21/Fake/episode\">Untrusted</a>"
      "<a href=\"https://myanimelist.net/anime/21/One_Piece/episode\">Episodes</a>"
      "<a href=\"/anime/210/Other_Show/episode\">Wrong anime</a>");
    QCOMPARE(ProviderClient::parseEpisodeListingUrlHtml(html, QStringLiteral("21")),
             QStringLiteral("https://myanimelist.net/anime/21/One_Piece/episode"));
    QVERIFY(ProviderClient::parseEpisodeListingUrlHtml(html, QStringLiteral("20")).isEmpty());
  }
};

QTEST_GUILESS_MAIN(ApiMappingTest)
#include "test_api_mapping.moc"
