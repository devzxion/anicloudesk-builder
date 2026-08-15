#include "ApiClient.h"

#include "BuildConfig.h"
#include "Database.h"
#include "SecureStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>
#include <QUuid>
#include <QSet>
#include <initializer_list>
#include <utility>

namespace {
const QByteArray UserAgent("AniCloudDesktop/" ANICLOUD_VERSION " (Qt; native)");
const QByteArray ProviderUserAgent(
  "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
  "(KHTML, like Gecko) Chrome/148.0.0.0 Safari/537.36");
const QString MalBaseUrl = QStringLiteral("https://myanimelist.net");
const QString MegaplayBaseUrl = QStringLiteral("https://megaplay.buzz");

QRegularExpression rx(const QString &pattern) {
  return QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption |
                            QRegularExpression::DotMatchesEverythingOption);
}

QString htmlText(const QString &value) {
  auto cleaned = value;
  cleaned.remove(rx(QStringLiteral("<script\\b[^>]*>.*?</script>")));
  cleaned.remove(rx(QStringLiteral("<style\\b[^>]*>.*?</style>")));
  return QTextDocumentFragment::fromHtml(cleaned).toPlainText().simplified();
}

QString capture(const QString &value, const QString &pattern, int group = 1) {
  const auto match = rx(pattern).match(value);
  return match.hasMatch() ? match.captured(group) : QString{};
}

QString htmlAttribute(const QString &tag, const QString &name) {
  const auto pattern = QStringLiteral("\\b%1\\s*=\\s*[\\\"']([^\\\"']*)[\\\"']")
                         .arg(QRegularExpression::escape(name));
  return htmlText(capture(tag, pattern));
}

QString normalizeImage(QString value) {
  value = htmlText(value).trimmed();
  if (value.startsWith(QStringLiteral("//"))) value.prepend(QStringLiteral("https:"));
  if (value.startsWith(QLatin1Char('/'))) value.prepend(MalBaseUrl);
  value.replace(rx(QStringLiteral("/r/\\d+x\\d+/")), QStringLiteral("/"));
  return value;
}

QString posterFrom(const QString &block) {
  auto it = rx(QStringLiteral("<img\\b[^>]*>")).globalMatch(block);
  while (it.hasNext()) {
    const auto tag = it.next().captured(0);
    for (const auto &attribute : {QStringLiteral("data-src"), QStringLiteral("data-lazy-src"),
                                  QStringLiteral("src"), QStringLiteral("data-srcset"),
                                  QStringLiteral("srcset")}) {
      auto value = htmlAttribute(tag, attribute);
      if (value.isEmpty() || value.contains(QStringLiteral("spacer.gif"))) continue;
      value = value.section(QLatin1Char(' '), 0, 0);
      if (value.startsWith(QStringLiteral("http")) || value.startsWith(QStringLiteral("//")))
        return normalizeImage(value);
    }
  }
  return {};
}

struct AnimeIdentity {
  QString id;
  QString title;
};

AnimeIdentity animeIdentity(const QString &block) {
  AnimeIdentity result;
  auto links = rx(QStringLiteral(
    "<a\\b([^>]*)href\\s*=\\s*[\\\"'][^\\\"']*/anime/(\\d+)(?:/[^\\\"']*)?[\\\"']([^>]*)>(.*?)</a>"))
                 .globalMatch(block);
  while (links.hasNext()) {
    const auto match = links.next();
    if (result.id.isEmpty()) result.id = match.captured(2);
    auto title = htmlText(match.captured(4));
    if (title.isEmpty()) {
      const auto attributes = match.captured(1) + match.captured(3);
      title = htmlAttribute(QStringLiteral("<a %1>").arg(attributes), QStringLiteral("title"));
    }
    if (!title.isEmpty()) {
      result.id = match.captured(2);
      result.title = title;
      break;
    }
  }
  return result;
}

int firstNumber(const QString &value, const QString &pattern) {
  return capture(value, pattern).toInt();
}

QVariantMap providerCard(const AnimeIdentity &identity, const QString &poster, int episodes,
                         const QString &type = QStringLiteral("TV"),
                         const QString &duration = QStringLiteral("N/A"),
                         const QString &synopsis = {}) {
  return QVariantMap{
    {QStringLiteral("id"), identity.id},
    {QStringLiteral("title"), identity.title},
    {QStringLiteral("alternativeTitle"), identity.title},
    {QStringLiteral("poster"), poster},
    {QStringLiteral("type"), type.isEmpty() ? QStringLiteral("TV") : type},
    {QStringLiteral("duration"), duration.isEmpty() ? QStringLiteral("N/A") : duration},
    {QStringLiteral("synopsis"), synopsis},
    {QStringLiteral("subEpisodes"), qMax(0, episodes)},
    {QStringLiteral("dubEpisodes"), 0},
    {QStringLiteral("episodes"), qMax(0, episodes)},
  };
}

QUrl providerUrl(const QString &base, const QString &path,
                 const QList<QPair<QString, QString>> &parameters = {}) {
  QUrl url(base + path);
  QUrlQuery query;
  for (const auto &[key, value] : parameters) query.addQueryItem(key, value);
  if (!parameters.isEmpty()) url.setQuery(query);
  return url;
}

QList<QPair<QByteArray, QByteArray>> malHeaders() {
  return {
    {QByteArrayLiteral("User-Agent"), ProviderUserAgent},
    {QByteArrayLiteral("Accept"), QByteArrayLiteral("text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8")},
    {QByteArrayLiteral("Accept-Language"), QByteArrayLiteral("en-US,en;q=0.9")},
    {QByteArrayLiteral("Referer"), QByteArrayLiteral("https://myanimelist.net/")},
  };
}

QList<QPair<QByteArray, QByteArray>> megaplayHeaders(const QUrl &referer = {}) {
  QList<QPair<QByteArray, QByteArray>> headers{
    {QByteArrayLiteral("User-Agent"), ProviderUserAgent},
    {QByteArrayLiteral("Accept-Language"), QByteArrayLiteral("en-US,en;q=0.9")},
    {QByteArrayLiteral("Referer"), referer.isValid() ? referer.toString().toUtf8()
                                                     : QByteArrayLiteral("https://megaplay.buzz/")},
  };
  return headers;
}

QString firstString(const QJsonObject &value, std::initializer_list<const char *> keys) {
  for (const auto *key : keys) {
    const auto candidate = value.value(QLatin1StringView(key)).toString();
    if (!candidate.isEmpty()) return candidate;
  }
  return {};
}

int firstInt(const QJsonObject &value, std::initializer_list<const char *> keys) {
  for (const auto *key : keys) {
    const auto candidate = value.value(QLatin1StringView(key));
    if (candidate.isDouble()) return candidate.toInt();
    if (candidate.isString()) return candidate.toString().toInt();
  }
  return 0;
}

QVariantMap card(const QJsonObject &value) {
  const auto episodes = value.value(QStringLiteral("episodes")).toObject();
  const auto title = value.value(QStringLiteral("title")).toObject();
  const auto cover = value.value(QStringLiteral("coverImage")).toObject();
  QVariantMap result = value.toVariantMap();
  result.insert(QStringLiteral("id"), firstString(value, {"id", "animeId", "mal_id"}));
  auto displayTitle = firstString(value, {"title", "name", "englishTitle"});
  if (displayTitle.isEmpty()) displayTitle = firstString(title, {"userPreferred", "english", "romaji", "native"});
  result.insert(QStringLiteral("title"), displayTitle);
  result.insert(QStringLiteral("alternativeTitle"), firstString(value, {"japaneseTitle", "alternativeTitle", "otherName", "other_name"}));
  auto poster = firstString(value, {"poster", "image", "img", "imageUrl", "cover"});
  if (poster.isEmpty()) poster = firstString(cover, {"extraLarge", "large", "medium"});
  result.insert(QStringLiteral("poster"), poster);
  result.insert(QStringLiteral("banner"), firstString(value, {"banner", "bannerImage"}));
  result.insert(QStringLiteral("synopsis"), firstString(value, {"description", "synopsis", "overview", "plot_summary"}));
  result.insert(QStringLiteral("subEpisodes"), episodes.value(QStringLiteral("sub")).toInt(firstInt(value, {"sub"})));
  result.insert(QStringLiteral("dubEpisodes"), episodes.value(QStringLiteral("dub")).toInt(firstInt(value, {"dub"})));
  result.insert(QStringLiteral("episodes"), episodes.value(QStringLiteral("eps")).toInt(firstInt(value, {"episodes", "episodeCount", "totalEpisodes"})));
  return result;
}

QJsonObject innerPayload(const QJsonObject &root) {
  auto value = root.value(QStringLiteral("data"));
  if (value.isObject()) return value.toObject();
  value = root.value(QStringLiteral("results"));
  if (value.isObject()) return value.toObject();
  return root;
}

QUrl apiUrl(const QString &base, const QString &path) {
  return QUrl(base + (path.startsWith(QLatin1Char('/')) ? path : QStringLiteral("/") + path));
}

QString networkMessage(QNetworkReply *reply, const QJsonObject &root) {
  const auto bodyMessage = firstString(root, {"message", "error"});
  if (!bodyMessage.isEmpty()) return bodyMessage;
  if (reply->error() != QNetworkReply::NoError) return reply->errorString();
  return QStringLiteral("AniCloud returned an invalid response.");
}
}

ProviderClient::ProviderClient(QObject *parent) : QObject(parent) {}

void ProviderClient::setError(const QString &error) {
  if (m_error == error) return;
  m_error = error;
  emit errorChanged();
}

QJsonObject ProviderClient::payload(const QJsonObject &root) { return innerPayload(root); }

void ProviderClient::getText(const QUrl &url,
                             const QList<QPair<QByteArray, QByteArray>> &headers,
                             TextSuccess success,
                             std::function<void(const QString &)> failure) {
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(75'000);
  for (const auto &[name, value] : headers) request.setRawHeader(name, value);
  const bool wasLoading = loading();
  ++m_pending;
  if (!wasLoading) emit loadingChanged();
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
    const auto body = reply->readAll();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && !body.isEmpty();
    if (ok) {
      setError({});
      success(body, reply->url());
    } else {
      const auto message = status > 0
        ? QStringLiteral("Anime provider returned HTTP %1.").arg(status)
        : reply->errorString();
      setError(message);
      if (failure) failure(message);
    }
    reply->deleteLater();
    --m_pending;
    if (m_pending == 0) emit loadingChanged();
  });
}

QVariantList ProviderClient::parseTopAnimeHtml(const QString &html, int limit) {
  QVariantList result;
  auto rows = rx(QStringLiteral(
    "<tr\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*ranking-list[^\\\"']*[\\\"'][^>]*>.*?</tr>"))
                .globalMatch(html);
  while (rows.hasNext() && result.size() < qMax(1, limit)) {
    const auto block = rows.next().captured(0);
    const auto identity = animeIdentity(block);
    if (identity.id.isEmpty() || identity.title.isEmpty()) continue;
    const auto episodes = firstNumber(block, QStringLiteral("\\((\\d+)\\s*eps\\)"));
    auto type = htmlText(capture(block, QStringLiteral("<div\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*information[^\\\"']*[\\\"'][^>]*>(.*?)</div>")));
    type = capture(type, QStringLiteral("^([A-Za-z0-9+.-]+)"));
    auto duration = capture(block, QStringLiteral("(\\d+)\\s*min"));
    if (!duration.isEmpty()) duration.append(QLatin1Char('m'));
    auto item = providerCard(identity, posterFrom(block), episodes, type, duration);
    item.insert(QStringLiteral("rank"), firstNumber(block, QStringLiteral("top-anime-rank-text[^>]*>(\\d+)")));
    result.append(item);
  }
  return result;
}

QVariantList ProviderClient::parseSeasonAnimeHtml(const QString &html, int page, int limit) {
  QVariantList all;
  const auto opener = rx(QStringLiteral(
    "<div\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*\\sjs-seasonal-anime(?=\\s|[\\\"'])[^\\\"']*[\\\"'][^>]*>"));
  QList<qsizetype> starts;
  auto matches = opener.globalMatch(html);
  while (matches.hasNext()) starts.append(matches.next().capturedStart());
  for (qsizetype index = 0; index < starts.size(); ++index) {
    const auto end = index + 1 < starts.size() ? starts.at(index + 1) : html.size();
    const auto block = html.mid(starts.at(index), end - starts.at(index));
    const auto identity = animeIdentity(block);
    if (identity.id.isEmpty() || identity.title.isEmpty()) continue;
    const auto episodes = firstNumber(block, QStringLiteral("(\\d+)\\s*eps"));
    auto duration = capture(block, QStringLiteral("(\\d+)\\s*min"));
    if (!duration.isEmpty()) duration.append(QLatin1Char('m'));
    auto type = QStringLiteral("TV");
    const auto classText = capture(block.left(500), QStringLiteral("class\\s*=\\s*[\\\"']([^\\\"']+)[\\\"']"));
    if (classText.contains(QStringLiteral("js-anime-type-2"))) type = QStringLiteral("Movie");
    else if (classText.contains(QStringLiteral("js-anime-type-3"))) type = QStringLiteral("OVA");
    else if (classText.contains(QStringLiteral("js-anime-type-4"))) type = QStringLiteral("Special");
    else if (classText.contains(QStringLiteral("js-anime-type-5"))) type = QStringLiteral("ONA");
    else if (classText.contains(QStringLiteral("js-anime-type-6"))) type = QStringLiteral("Music");
    all.append(providerCard(identity, posterFrom(block), episodes, type, duration));
  }
  const auto pageSize = qMax(1, limit);
  const auto start = (qMax(1, page) - 1) * pageSize;
  return all.mid(start, pageSize);
}

QVariantList ProviderClient::parseSearchHtml(const QString &html, int limit) {
  QVariantList result;
  QSet<QString> seen;
  auto rows = rx(QStringLiteral("<tr\\b[^>]*>.*?</tr>")).globalMatch(html);
  while (rows.hasNext() && result.size() < qMax(1, limit)) {
    const auto block = rows.next().captured(0);
    if (!block.contains(QStringLiteral("hoverinfo_trigger"), Qt::CaseInsensitive)) continue;
    const auto identity = animeIdentity(block);
    if (identity.id.isEmpty() || identity.title.isEmpty() || seen.contains(identity.id)) continue;
    auto cells = rx(QStringLiteral("<td\\b[^>]*>(.*?)</td>")).globalMatch(block);
    QStringList textCells;
    while (cells.hasNext()) textCells.append(htmlText(cells.next().captured(1)));
    if (textCells.size() < 5) continue;
    seen.insert(identity.id);
    result.append(providerCard(identity, posterFrom(block), textCells.at(3).toInt(),
                               textCells.at(2), QStringLiteral("N/A")));
  }
  return result;
}

QVariantMap ProviderClient::parseAnimeDetailsHtml(const QString &html, const QString &animeId,
                                                  QVariantList *recommendations) {
  auto title = htmlText(capture(html, QStringLiteral(
    "<h1\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*title-name[^\\\"']*[\\\"'][^>]*>(.*?)</h1>")));
  if (title.isEmpty()) title = htmlText(capture(html, QStringLiteral("<title>(.*?)</title>"))).section(QStringLiteral(" - "), 0, 0);
  auto alternativeTitle = htmlText(capture(html, QStringLiteral(
    "<(?:p|div)\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*title-english[^\\\"']*[\\\"'][^>]*>(.*?)</(?:p|div)>")));
  if (alternativeTitle.isEmpty()) alternativeTitle = title;

  QMap<QString, QString> details;
  auto detailRows = rx(QStringLiteral(
    "<(?:div|span)\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*spaceit_pad[^\\\"']*[\\\"'][^>]*>(.*?)</(?:div|span)>"))
                      .globalMatch(html);
  while (detailRows.hasNext()) {
    const auto block = detailRows.next().captured(1);
    auto key = htmlText(capture(block, QStringLiteral(
      "<span\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*dark_text[^\\\"']*[\\\"'][^>]*>(.*?)</span>")));
    key.remove(QLatin1Char(':'));
    key = key.trimmed();
    if (key.isEmpty()) continue;
    auto value = htmlText(block);
    if (value.startsWith(key)) value = value.mid(key.size()).remove(QRegularExpression(QStringLiteral("^:\\s*")));
    details.insert(key, value.trimmed());
  }

  auto posterTag = capture(html, QStringLiteral("<img\\b[^>]*itemprop\\s*=\\s*[\\\"']image[\\\"'][^>]*>"), 0);
  auto poster = posterFrom(posterTag);
  if (poster.isEmpty()) poster = posterFrom(capture(html, QStringLiteral(
    "<div\\b[^>]*class\\s*=\\s*[\\\"'][^\\\"']*leftside[^\\\"']*[\\\"'][^>]*>(.*?)</div>")));
  const auto synopsis = htmlText(capture(html, QStringLiteral(
    "<(?:p|span)\\b[^>]*itemprop\\s*=\\s*[\\\"']description[\\\"'][^>]*>(.*?)</(?:p|span)>")));
  const auto episodes = firstNumber(details.value(QStringLiteral("Episodes")), QStringLiteral("(\\d+)"));
  auto result = providerCard({animeId, title}, poster, episodes,
                             details.value(QStringLiteral("Type")),
                             details.value(QStringLiteral("Duration")), synopsis);
  result.insert(QStringLiteral("alternativeTitle"), alternativeTitle);
  result.insert(QStringLiteral("status"), details.value(QStringLiteral("Status")));
  result.insert(QStringLiteral("rating"), details.value(QStringLiteral("Rating")));
  result.insert(QStringLiteral("premiered"), details.value(QStringLiteral("Premiered")));
  result.insert(QStringLiteral("aired"), details.value(QStringLiteral("Aired")));
  result.insert(QStringLiteral("score"), details.value(QStringLiteral("Score")));

  if (recommendations) {
    recommendations->clear();
    QSet<QString> seen{animeId};
    const auto recommendationStart = html.indexOf(QStringLiteral("anime_recommendation"), 0, Qt::CaseInsensitive);
    if (recommendationStart < 0) return result;
    auto recommendationHtml = html.mid(recommendationStart);
    auto anchors = rx(QStringLiteral(
      "<a\\b([^>]*)href\\s*=\\s*[\\\"'][^\\\"']*/anime/(\\d+)(?:/[^\\\"']*)?[\\\"']([^>]*)>(.*?)</a>"))
                   .globalMatch(recommendationHtml);
    while (anchors.hasNext() && recommendations->size() < 24) {
      const auto match = anchors.next();
      const auto id = match.captured(2);
      if (seen.contains(id)) continue;
      auto candidateTitle = htmlText(match.captured(4));
      if (candidateTitle.isEmpty()) {
        const auto attributes = QStringLiteral("<a %1 %2>").arg(match.captured(1), match.captured(3));
        candidateTitle = htmlAttribute(attributes, QStringLiteral("title"));
      }
      if (candidateTitle.isEmpty()) continue;
      seen.insert(id);
      recommendations->append(providerCard({id, candidateTitle}, QString{}, 0));
    }
  }
  return result;
}

QVariantList ProviderClient::cardList(const QJsonValue &value) {
  QJsonArray array;
  if (value.isArray()) array = value.toArray();
  if (value.isObject()) {
    const auto object = value.toObject();
    for (const auto &key : {QStringLiteral("items"), QStringLiteral("results"), QStringLiteral("animes"), QStringLiteral("data")}) {
      if (object.value(key).isArray()) { array = object.value(key).toArray(); break; }
    }
  }
  QVariantList result;
  result.reserve(array.size());
  for (const auto &item : array) if (item.isObject()) result.append(card(item.toObject()));
  return result;
}

QVariantList ProviderClient::episodeList(const QJsonValue &value, const QString &animeId) {
  QJsonArray array = value.toArray();
  if (value.isObject()) {
    const auto object = value.toObject();
    array = object.value(QStringLiteral("episodes")).toArray();
    if (array.isEmpty()) array = object.value(QStringLiteral("items")).toArray();
    if (array.isEmpty()) array = object.value(QStringLiteral("results")).toArray();
  }
  QVariantList result;
  result.reserve(array.size());
  for (const auto &item : array) {
    if (item.isArray()) {
      const auto tuple = item.toArray();
      if (tuple.size() < 2) continue;
      const auto number = tuple.at(0).toVariant().toInt();
      result.append(QVariantMap{
        {QStringLiteral("id"), tuple.at(1).toString()}, {QStringLiteral("animeId"), animeId},
        {QStringLiteral("number"), number}, {QStringLiteral("title"), QStringLiteral("Episode %1").arg(number)},
      });
      continue;
    }
    if (!item.isObject()) continue;
    const auto object = item.toObject();
    QVariantMap episode = object.toVariantMap();
    episode.insert(QStringLiteral("id"), firstString(object, {"id", "episodeId"}));
    episode.insert(QStringLiteral("animeId"), animeId);
    episode.insert(QStringLiteral("number"), firstInt(object, {"number", "episode", "episodeNumber"}));
    episode.insert(QStringLiteral("title"), firstString(object, {"title", "name"}));
    result.append(episode);
  }
  return result;
}

void ProviderClient::loadHome() {
  setError({});
  loadCategory(QStringLiteral("recent"), 1);
  loadCategory(QStringLiteral("popular"), 1);
  loadCategory(QStringLiteral("airing"), 1);
}

void ProviderClient::loadCategory(const QString &kind, int page) {
  QUrl url;
  if (kind == QStringLiteral("recent")) {
    url = providerUrl(MalBaseUrl, QStringLiteral("/anime/season"));
  } else {
    url = providerUrl(MalBaseUrl, QStringLiteral("/topanime.php"), {
      {QStringLiteral("type"), kind == QStringLiteral("popular") ? QStringLiteral("bypopularity")
                                                                  : QStringLiteral("airing")},
      {QStringLiteral("limit"), QString::number(qMax(0, page - 1) * 50)},
    });
  }
  getText(url, malHeaders(), [this, kind, page](const QByteArray &body, const QUrl &) {
    const auto html = QString::fromUtf8(body);
    auto items = kind == QStringLiteral("recent")
      ? parseSeasonAnimeHtml(html, page, 20)
      : parseTopAnimeHtml(html, 20);
    if (kind == QStringLiteral("recent")) m_recent = items;
    else if (kind == QStringLiteral("popular")) {
      m_popular = items;
    } else {
      m_airing = items;
      m_spotlight = items.mid(0, 10);
    }
    emit homeChanged();
  });
}

void ProviderClient::search(const QString &query, int page) {
  if (query.trimmed().isEmpty()) {
    m_searchResults.clear(); m_searchHasMore = false; emit searchChanged(); return;
  }
  const auto currentPage = qMax(1, page);
  const auto url = providerUrl(MalBaseUrl, QStringLiteral("/anime.php"), {
    {QStringLiteral("q"), query.trimmed()},
    {QStringLiteral("cat"), QStringLiteral("anime")},
    {QStringLiteral("show"), QString::number((currentPage - 1) * 20)},
  });
  getText(url, malHeaders(), [this, currentPage](const QByteArray &body, const QUrl &) {
    const auto items = parseSearchHtml(QString::fromUtf8(body), 20);
    if (currentPage <= 1) m_searchResults = items; else m_searchResults.append(items);
    m_searchHasMore = items.size() >= 20;
    emit searchChanged();
  });
}

void ProviderClient::loadDetails(const QString &animeId) {
  m_details.clear(); m_episodes.clear(); m_recommendations.clear(); emit detailsChanged();
  const auto url = providerUrl(MalBaseUrl, QStringLiteral("/anime/%1").arg(
    QString::fromUtf8(QUrl::toPercentEncoding(animeId))));
  getText(url, malHeaders(), [this, animeId](const QByteArray &body, const QUrl &) {
    m_details = parseAnimeDetailsHtml(QString::fromUtf8(body), animeId, &m_recommendations);
    applyEpisodes(animeId, qMax(1, m_details.value(QStringLiteral("episodes")).toInt()));
    emit detailsChanged();
  });
}

void ProviderClient::applyEpisodes(const QString &animeId, int episodeCount) {
  m_episodes.clear();
  for (int number = 1; number <= qBound(1, episodeCount, 3000); ++number) {
    m_episodes.append(QVariantMap{
      {QStringLiteral("id"), QStringLiteral("%1::ep=%2").arg(animeId).arg(number)},
      {QStringLiteral("animeId"), animeId},
      {QStringLiteral("number"), number},
      {QStringLiteral("title"), QStringLiteral("Episode %1").arg(number)},
    });
  }
}

void ProviderClient::loadEpisodes(const QString &animeId, int offset, int limit) {
  Q_UNUSED(offset)
  Q_UNUSED(limit)
  if (m_details.value(QStringLiteral("id")).toString() == animeId &&
      m_details.value(QStringLiteral("episodes")).toInt() > 0) {
    applyEpisodes(animeId, m_details.value(QStringLiteral("episodes")).toInt());
    emit detailsChanged();
    return;
  }
  loadDetails(animeId);
}

void ProviderClient::loadServers(const QString &episodeId) {
  Q_UNUSED(episodeId)
  m_subServers = {
    QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("type"), QStringLiteral("sub")}, {QStringLiteral("id"), 1}, {QStringLiteral("name"), QStringLiteral("hd-1")}},
    QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("type"), QStringLiteral("sub")}, {QStringLiteral("id"), 2}, {QStringLiteral("name"), QStringLiteral("hd-2")}},
  };
  m_dubServers = {
    QVariantMap{{QStringLiteral("index"), 0}, {QStringLiteral("type"), QStringLiteral("dub")}, {QStringLiteral("id"), 1}, {QStringLiteral("name"), QStringLiteral("hd-1")}},
    QVariantMap{{QStringLiteral("index"), 1}, {QStringLiteral("type"), QStringLiteral("dub")}, {QStringLiteral("id"), 2}, {QStringLiteral("name"), QStringLiteral("hd-2")}},
  };
  emit serversChanged();
}

QVariantMap ProviderClient::streamMap(const QJsonObject &root, const QString &episodeId,
                                      const QString &server, const QString &audioMode) {
  auto data = payload(root);
  if (data.value(QStringLiteral("stream")).isObject()) data = data.value(QStringLiteral("stream")).toObject();
  QVariantMap result;
  result.insert(QStringLiteral("episodeId"), episodeId);
  result.insert(QStringLiteral("server"), firstString(data, {"server"}).isEmpty() ? server : firstString(data, {"server"}));
  result.insert(QStringLiteral("audioMode"), audioMode);
  const auto referer = firstString(data, {"Referer", "referer", "referrer"});
  result.insert(QStringLiteral("referer"), referer);
  QVariantMap headers = data.value(QStringLiteral("headers")).toObject().toVariantMap();
  if (!referer.isEmpty()) headers.insert(QStringLiteral("Referer"), referer);
  auto origin = firstString(data, {"Origin", "origin"});
  if (origin.isEmpty() && !referer.isEmpty()) {
    const QUrl refererUrl(referer);
    if (refererUrl.isValid() && !refererUrl.scheme().isEmpty() && !refererUrl.authority().isEmpty())
      origin = QStringLiteral("%1://%2").arg(refererUrl.scheme(), refererUrl.authority());
  }
  if (!origin.isEmpty()) headers.insert(QStringLiteral("Origin"), origin);
  headers.insert(QStringLiteral("User-Agent"), QString::fromUtf8(UserAgent));
  result.insert(QStringLiteral("headers"), headers);

  auto sources = data.value(QStringLiteral("sources")).toArray();
  if (sources.isEmpty() && data.value(QStringLiteral("source")).isArray()) sources = data.value(QStringLiteral("source")).toArray();
  if (sources.isEmpty() && data.value(QStringLiteral("sources")).isObject()) sources.append(data.value(QStringLiteral("sources")));
  QVariantList alternates;
  QString selected;
  for (const auto &sourceValue : sources) {
    const auto source = sourceValue.toObject();
    const auto url = firstString(source, {"file", "url", "src"});
    if (url.isEmpty()) continue;
    QVariantMap item = source.toVariantMap();
    item.insert(QStringLiteral("url"), url);
    alternates.append(item);
    const auto label = source.value(QStringLiteral("label")).toString().toLower();
    if (selected.isEmpty() || label.contains(QStringLiteral("auto"))) selected = url;
  }
  if (selected.isEmpty()) selected = firstString(data, {"directFile", "file", "url", "link"});
  result.insert(QStringLiteral("mediaUrl"), selected);
  result.insert(QStringLiteral("alternates"), alternates);

  QVariantList subtitles;
  for (const auto &trackValue : data.value(QStringLiteral("tracks")).toArray()) {
    const auto track = trackValue.toObject();
    const auto kind = track.value(QStringLiteral("kind")).toString().toLower();
    if (!kind.isEmpty() && kind != QStringLiteral("captions") && kind != QStringLiteral("subtitles")) continue;
    QVariantMap item = track.toVariantMap();
    item.insert(QStringLiteral("url"), firstString(track, {"file", "url", "src"}));
    subtitles.append(item);
  }
  result.insert(QStringLiteral("subtitles"), subtitles);
  const auto intro = data.value(QStringLiteral("intro")).toObject();
  const auto outro = data.value(QStringLiteral("outro")).toObject();
  result.insert(QStringLiteral("introStart"), firstInt(intro, {"start"}));
  result.insert(QStringLiteral("introEnd"), firstInt(intro, {"end"}));
  result.insert(QStringLiteral("outroStart"), firstInt(outro, {"start"}));
  result.insert(QStringLiteral("outroEnd"), firstInt(outro, {"end"}));
  return result;
}

void ProviderClient::resolveStream(int generation, const QString &episodeId,
                                   const QString &server, const QString &audioMode) {
  resolveStreamPage(generation, episodeId, server, audioMode, true);
}

void ProviderClient::resolveStreamPage(int generation, const QString &episodeId,
                                       const QString &server, const QString &audioMode,
                                       bool allowFallback) {
  const auto match = QRegularExpression(QStringLiteral("^(.+)::ep=(\\d+)$"),
                                         QRegularExpression::CaseInsensitiveOption).match(episodeId);
  if (!match.hasMatch()) {
    const auto message = QStringLiteral("This episode has an invalid bundled-provider identifier.");
    setError(message); emit streamFailed(generation, message); return;
  }
  const auto animeId = match.captured(1);
  const auto episodeNumber = qMax(1, match.captured(2).toInt());
  const auto normalizedServer = server == QStringLiteral("hd-2") ? QStringLiteral("hd-2") : QStringLiteral("hd-1");
  const auto mode = normalizedServer == QStringLiteral("hd-1") ? QStringLiteral("ani") : QStringLiteral("mal");
  const auto normalizedAudio = audioMode == QStringLiteral("dub") ? QStringLiteral("dub") : QStringLiteral("sub");
  const auto streamPage = providerUrl(MegaplayBaseUrl,
    QStringLiteral("/stream/%1/%2/%3/%4").arg(mode, animeId, QString::number(episodeNumber), normalizedAudio));

  const auto fallback = [this, generation, episodeId, normalizedServer, normalizedAudio, allowFallback](const QString &message) {
    if (allowFallback) {
      resolveStreamPage(generation, episodeId,
                        normalizedServer == QStringLiteral("hd-1") ? QStringLiteral("hd-2") : QStringLiteral("hd-1"),
                        normalizedAudio, false);
    } else {
      emit streamFailed(generation, message);
    }
  };

  getText(streamPage, megaplayHeaders(),
          [this, generation, episodeId, normalizedServer, normalizedAudio, streamPage, fallback]
          (const QByteArray &body, const QUrl &) {
    const auto html = QString::fromUtf8(body);
    auto sourceId = capture(html, QStringLiteral("data-id\\s*=\\s*[\\\"']([A-Za-z0-9_-]+)[\\\"']"));
    if (sourceId.isEmpty())
      sourceId = capture(html, QStringLiteral("/stream/getSources\\?id=([A-Za-z0-9_-]+)"));
    if (sourceId.isEmpty()) {
      fallback(QStringLiteral("This server has no stream for the selected episode."));
      return;
    }
    const auto sourcesUrl = providerUrl(MegaplayBaseUrl, QStringLiteral("/stream/getSources"),
                                         {{QStringLiteral("id"), sourceId}});
    auto headers = megaplayHeaders(streamPage);
    headers.append({QByteArrayLiteral("X-Requested-With"), QByteArrayLiteral("XMLHttpRequest")});
    headers.append({QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json,text/plain,*/*")});
    getText(sourcesUrl, headers,
            [this, generation, episodeId, normalizedServer, normalizedAudio, fallback]
            (const QByteArray &json, const QUrl &) {
      const auto document = QJsonDocument::fromJson(json);
      if (!document.isObject()) {
        fallback(QStringLiteral("The stream provider returned invalid data."));
        return;
      }
      auto root = document.object();
      if (root.value(QStringLiteral("data")).isObject()) root = root.value(QStringLiteral("data")).toObject();
      root.insert(QStringLiteral("server"), normalizedServer);
      root.insert(QStringLiteral("referer"), QStringLiteral("https://megaplay.buzz/"));
      const auto stream = streamMap(root, episodeId, normalizedServer, normalizedAudio);
      if (stream.value(QStringLiteral("mediaUrl")).toString().isEmpty())
        fallback(QStringLiteral("This server did not return a playable stream."));
      else
        emit streamResolved(generation, stream);
    }, fallback);
  }, fallback);
}

AccountClient::AccountClient(SecureStore *secureStore, Database *database, QObject *parent)
  : QObject(parent), m_secureStore(secureStore), m_database(database) {
  if (auto *network = QNetworkInformation::instance()) {
    connect(network, &QNetworkInformation::reachabilityChanged, this,
            [this](QNetworkInformation::Reachability reachability) {
      if (reachability == QNetworkInformation::Reachability::Online) flushPendingProgress();
    });
  }
}

QJsonObject AccountClient::payload(const QJsonObject &root) { return innerPayload(root); }

QVariantList AccountClient::objectList(const QJsonValue &value) {
  QVariantList result;
  for (const auto &item : value.toArray()) if (item.isObject()) result.append(item.toObject().toVariantMap());
  return result;
}

void AccountClient::setError(const QString &error) {
  if (m_error == error) return;
  m_error = error;
  emit errorChanged();
}

void AccountClient::setToken(const QString &token) {
  const bool before = authenticated();
  m_token = token;
  QString ignored;
  if (token.isEmpty()) m_secureStore->remove(QStringLiteral("account-token-v1"), &ignored);
  else m_secureStore->writeText(QStringLiteral("account-token-v1"), token, &ignored);
  if (before != authenticated()) emit authenticationChanged();
}

void AccountClient::request(const QString &path, const QByteArray &method, const QJsonObject &body,
                            bool needsAuthentication, Success success,
                            std::function<void(const QString &, int)> failure) {
  QNetworkRequest request(apiUrl(QString::fromUtf8(ANICLOUD_ACCOUNT_API_BASE_URL), path));
  request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
  request.setRawHeader(QByteArrayLiteral("User-Agent"), UserAgent);
  if (needsAuthentication && !m_token.isEmpty())
    request.setRawHeader(QByteArrayLiteral("Authorization"), QByteArrayLiteral("Bearer ") + m_token.toUtf8());
  const bool wasBusy = busy(); ++m_pending; if (!wasBusy) emit busyChanged();
  QNetworkReply *reply = nullptr;
  const auto bytes = body.isEmpty() ? QByteArrayLiteral("{}") : QJsonDocument(body).toJson(QJsonDocument::Compact);
  if (method == QByteArrayLiteral("GET")) reply = m_network.get(request);
  else if (method == QByteArrayLiteral("DELETE") && body.isEmpty()) reply = m_network.deleteResource(request);
  else reply = m_network.sendCustomRequest(request, method, bytes);
  connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto root = doc.object();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool apiSuccess = !root.contains(QStringLiteral("success")) || root.value(QStringLiteral("success")).toBool();
    if (reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && apiSuccess) {
      setError({}); success(root);
    } else {
      const auto message = networkMessage(reply, root); setError(message);
      if (status == 401 && authenticated()) { setToken({}); m_user.clear(); emit authenticationChanged(); emit sessionExpired(); }
      if (failure) failure(message, status);
    }
    reply->deleteLater(); --m_pending; if (m_pending == 0) emit busyChanged();
  });
}

void AccountClient::authenticateFrom(const QJsonObject &root) {
  const auto data = payload(root);
  setToken(firstString(data, {"token", "accessToken", "access_token"}));
  m_user = data.value(QStringLiteral("user")).toObject().toVariantMap();
  emit authenticationChanged();
  emit operationSucceeded(QStringLiteral("Signed in."));
  refreshLibrary();
  flushPendingProgress();
}

void AccountClient::restoreSession() {
  QString error;
  const auto token = m_secureStore->readText(QStringLiteral("account-token-v1"), &error);
  if (token.isEmpty()) return;
  m_token = token; emit authenticationChanged();
  request(QStringLiteral("/auth/me"), QByteArrayLiteral("GET"), {}, true, [this](const QJsonObject &root) {
    m_user = payload(root).value(QStringLiteral("user")).toObject().toVariantMap();
    emit authenticationChanged(); refreshLibrary(); flushPendingProgress();
  });
}

void AccountClient::registerAccount(const QString &name, const QString &email, const QString &password) {
  request(QStringLiteral("/auth/register"), QByteArrayLiteral("POST"),
          {{QStringLiteral("name"), name}, {QStringLiteral("email"), email}, {QStringLiteral("password"), password}}, false,
          [this](const QJsonObject &root) {
    const auto data = payload(root);
    emit verificationRequired(data.value(QStringLiteral("email")).toString(), data.value(QStringLiteral("otpExpiresInMinutes")).toInt(15));
  });
}

void AccountClient::verifyEmail(const QString &email, const QString &otp) {
  request(QStringLiteral("/auth/verify-email"), QByteArrayLiteral("POST"),
          {{QStringLiteral("email"), email}, {QStringLiteral("otp"), otp}}, false,
          [this](const QJsonObject &root) { authenticateFrom(root); });
}

void AccountClient::resendVerification(const QString &email) {
  request(QStringLiteral("/auth/resend-verification-otp"), QByteArrayLiteral("POST"), {{QStringLiteral("email"), email}}, false,
          [this, email](const QJsonObject &) { emit verificationRequired(email, 15); });
}

void AccountClient::login(const QString &email, const QString &password) {
  request(QStringLiteral("/auth/login"), QByteArrayLiteral("POST"),
          {{QStringLiteral("email"), email}, {QStringLiteral("password"), password}}, false,
          [this](const QJsonObject &root) { authenticateFrom(root); });
}

void AccountClient::forgotPassword(const QString &email) {
  request(QStringLiteral("/auth/forgot-password"), QByteArrayLiteral("POST"), {{QStringLiteral("email"), email}}, false,
          [this, email](const QJsonObject &) { emit passwordResetRequested(email); });
}

void AccountClient::resetPassword(const QString &email, const QString &otp, const QString &newPassword) {
  request(QStringLiteral("/auth/reset-password"), QByteArrayLiteral("POST"),
          {{QStringLiteral("email"), email}, {QStringLiteral("otp"), otp}, {QStringLiteral("newPassword"), newPassword}}, false,
          [this](const QJsonObject &) { emit operationSucceeded(QStringLiteral("Password reset. You can sign in now.")); });
}

void AccountClient::logout() {
  if (authenticated()) request(QStringLiteral("/auth/logout"), QByteArrayLiteral("POST"), {}, true, [](const QJsonObject &) {});
  setToken({}); m_user.clear(); m_watchlist.clear(); m_history.clear(); m_completed.clear();
  emit authenticationChanged(); emit libraryChanged();
}

void AccountClient::refreshLibrary() {
  if (!authenticated()) return;
  loadPlaybackPreference();
  request(QStringLiteral("/watchlist"), QByteArrayLiteral("GET"), {}, true, [this](const QJsonObject &root) {
    m_watchlist = objectList(payload(root).value(QStringLiteral("items"))); emit libraryChanged();
  });
  request(QStringLiteral("/history?limit=200"), QByteArrayLiteral("GET"), {}, true, [this](const QJsonObject &root) {
    m_history = objectList(payload(root).value(QStringLiteral("records"))); emit libraryChanged();
  });
  request(QStringLiteral("/history/completed?limit=50"), QByteArrayLiteral("GET"), {}, true, [this](const QJsonObject &root) {
    m_completed = objectList(payload(root).value(QStringLiteral("records"))); emit libraryChanged();
  });
}

void AccountClient::addToWatchlist(const QVariantMap &anime) {
  QJsonObject body{{QStringLiteral("animeId"), anime.value(QStringLiteral("id")).toString()},
                   {QStringLiteral("title"), anime.value(QStringLiteral("title")).toString()},
                   {QStringLiteral("image"), anime.value(QStringLiteral("poster")).toString()},
                   {QStringLiteral("releaseDate"), anime.value(QStringLiteral("releaseDate")).toString()}};
  request(QStringLiteral("/watchlist"), QByteArrayLiteral("PUT"), body, true,
          [this](const QJsonObject &) { refreshLibrary(); });
}

void AccountClient::removeFromWatchlist(const QString &animeId) {
  request(QStringLiteral("/watchlist/%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(animeId))), QByteArrayLiteral("DELETE"), {}, true,
          [this](const QJsonObject &) { refreshLibrary(); });
}

void AccountClient::deleteHistory(const QString &recordId) {
  request(QStringLiteral("/history/%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(recordId))), QByteArrayLiteral("DELETE"), {}, true,
          [this](const QJsonObject &) { refreshLibrary(); });
}

void AccountClient::saveProgress(const QVariantMap &progress) {
  const auto key = progress.value(QStringLiteral("episodeId")).toString();
  if (key.isEmpty()) return;
  if (!authenticated()) { if (m_database->saveLocalProgress(progress)) emit localProgressSaved(); return; }
  const auto ownerId = m_user.value(QStringLiteral("id")).toString();
  const auto queueKey = ownerId + QLatin1Char(':') + key;
  const QVariantMap cloudProgress{
    {QStringLiteral("animeId"), progress.value(QStringLiteral("animeId"))},
    {QStringLiteral("animeName"), progress.value(QStringLiteral("animeName"))},
    {QStringLiteral("animeImage"), progress.value(QStringLiteral("animeImage"))},
    {QStringLiteral("episodeId"), progress.value(QStringLiteral("episodeId"))},
    {QStringLiteral("episodeName"), progress.value(QStringLiteral("episodeName"))},
    {QStringLiteral("audioMode"), progress.value(QStringLiteral("audioMode"))},
    {QStringLiteral("timestamp"), progress.value(QStringLiteral("positionSeconds"))},
    {QStringLiteral("duration"), progress.value(QStringLiteral("durationSeconds"))},
  };
  request(QStringLiteral("/history/progress"), QByteArrayLiteral("PUT"), QJsonObject::fromVariantMap(cloudProgress), true,
          [this, queueKey](const QJsonObject &) { m_database->removePendingProgress(queueKey); },
          [this, queueKey, ownerId, cloudProgress](const QString &, int status) {
    if (status == 0 || status >= 500) m_database->queueProgress(queueKey, ownerId, cloudProgress);
  });
}

void AccountClient::loadResumeEpisode(const QString &episodeId) {
  request(QStringLiteral("/history/resume?episodeId=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(episodeId))),
          QByteArrayLiteral("GET"), {}, true,
          [this](const QJsonObject &root) { emit resumeLoaded(QStringLiteral("episode"), payload(root).toVariantMap()); });
}

void AccountClient::loadResumeAnime(const QString &animeId) {
  request(QStringLiteral("/history/resume-anime?animeId=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(animeId))),
          QByteArrayLiteral("GET"), {}, true,
          [this](const QJsonObject &root) { emit resumeLoaded(QStringLiteral("anime"), payload(root).toVariantMap()); });
}

void AccountClient::loadPlaybackPreference() {
  if (!authenticated()) return;
  request(QStringLiteral("/preferences/playback-quality"), QByteArrayLiteral("GET"), {}, true,
          [this](const QJsonObject &root) {
    m_playbackPreference = payload(root).value(QStringLiteral("preference")).toObject().toVariantMap();
    emit preferenceChanged();
  });
}

void AccountClient::setPlaybackPreference(const QVariantMap &preference) {
  if (!authenticated()) return;
  request(QStringLiteral("/preferences/playback-quality"), QByteArrayLiteral("PUT"),
          {{QStringLiteral("preference"), QJsonObject::fromVariantMap(preference)}}, true,
          [this](const QJsonObject &root) {
    m_playbackPreference = payload(root).value(QStringLiteral("preference")).toObject().toVariantMap();
    emit preferenceChanged();
  });
}

void AccountClient::flushPendingProgress() {
  if (!authenticated()) return;
  const auto owner = m_user.value(QStringLiteral("id")).toString();
  for (const auto &entryValue : m_database->pendingProgress(owner)) {
    const auto entry = entryValue.toMap();
    const auto payloadMap = entry.value(QStringLiteral("payload")).toMap();
    const auto key = entry.value(QStringLiteral("key")).toString();
    request(QStringLiteral("/history/progress"), QByteArrayLiteral("PUT"), QJsonObject::fromVariantMap(payloadMap), true,
            [this, key](const QJsonObject &) { m_database->removePendingProgress(key); });
  }
}

void AccountClient::refreshBroadcasts() {
  request(QStringLiteral("/broadcasts/latest"), QByteArrayLiteral("GET"), {}, false, [this](const QJsonObject &root) {
    const auto broadcast = payload(root).value(QStringLiteral("broadcast"));
    QVariantList items;
    if (broadcast.isObject()) items.append(broadcast.toObject().toVariantMap());
    m_database->replaceBroadcasts(items); m_broadcasts = m_database->broadcasts(); emit broadcastsChanged();
  });
}

void AccountClient::markBroadcastRead(const QString &id) {
  if (m_database->markBroadcastRead(id)) {
    m_broadcasts = m_database->broadcasts();
    emit broadcastsChanged();
  }
}

void AccountClient::checkMaintenance() {
  request(QStringLiteral("/maintenance/status"), QByteArrayLiteral("GET"), {}, false, [this](const QJsonObject &root) {
    m_maintenance = payload(root).value(QStringLiteral("maintenance")).toObject().toVariantMap(); emit maintenanceChanged();
  });
}
