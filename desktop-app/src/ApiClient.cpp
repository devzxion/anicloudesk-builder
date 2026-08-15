#include "ApiClient.h"

#include "BuildConfig.h"
#include "Database.h"
#include "SecureStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkInformation>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <QUuid>
#include <initializer_list>
#include <utility>

namespace {
const QByteArray UserAgent("AniCloudDesktop/4.0.0 (Qt; native)");

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

void ProviderClient::get(const QString &path, Success success, std::function<void(const QString &)> failure) {
  QNetworkRequest request(apiUrl(QString::fromUtf8(ANICLOUD_PROVIDER_API_BASE_URL), path));
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
  request.setRawHeader(QByteArrayLiteral("User-Agent"), UserAgent);
  const bool wasLoading = loading();
  ++m_pending;
  if (!wasLoading) emit loadingChanged();
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, success = std::move(success), failure = std::move(failure)] {
    const auto doc = QJsonDocument::fromJson(reply->readAll());
    const auto root = doc.object();
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool ok = reply->error() == QNetworkReply::NoError && status >= 200 && status < 300 && doc.isObject();
    if (ok) {
      setError({});
      success(root);
    } else {
      const auto message = networkMessage(reply, root);
      setError(message);
      if (failure) failure(message);
    }
    reply->deleteLater();
    --m_pending;
    if (m_pending == 0) emit loadingChanged();
  });
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
  get(QStringLiteral("/home"), [this](const QJsonObject &root) {
    const auto data = payload(root);
    m_spotlight = cardList(data.value(QStringLiteral("spotlight")));
    if (m_spotlight.isEmpty()) m_spotlight = cardList(data.value(QStringLiteral("spotlightAnimes")));
    if (m_spotlight.isEmpty()) m_spotlight = cardList(data.value(QStringLiteral("anilistTrending")));
    auto recent = cardList(data.value(QStringLiteral("recent")));
    if (recent.isEmpty()) recent = cardList(data.value(QStringLiteral("latestEpisodes")));
    auto popular = cardList(data.value(QStringLiteral("popular")));
    if (popular.isEmpty()) popular = cardList(data.value(QStringLiteral("gogoPopular")));
    const auto airing = cardList(data.value(QStringLiteral("topAiring")));
    if (!recent.isEmpty()) m_recent = recent;
    if (!popular.isEmpty()) m_popular = popular;
    if (!airing.isEmpty()) m_airing = airing;
    emit homeChanged();
  });
  loadCategory(QStringLiteral("recent"), 1);
  loadCategory(QStringLiteral("popular"), 1);
  loadCategory(QStringLiteral("airing"), 1);
}

void ProviderClient::loadCategory(const QString &kind, int page) {
  QString path;
  if (kind == QStringLiteral("recent")) path = QStringLiteral("/recent/%1").arg(page);
  else if (kind == QStringLiteral("popular")) path = QStringLiteral("/gogoPopular/%1").arg(page);
  else path = QStringLiteral("/topAiring/%1").arg(page);
  get(path, [this, kind](const QJsonObject &root) {
    auto items = cardList(root.value(QStringLiteral("results")));
    if (items.isEmpty()) items = cardList(payload(root));
    if (kind == QStringLiteral("recent")) m_recent = items;
    else if (kind == QStringLiteral("popular")) m_popular = items;
    else m_airing = items;
    emit homeChanged();
  });
}

void ProviderClient::search(const QString &query, int page) {
  if (query.trimmed().isEmpty()) {
    m_searchResults.clear(); m_searchHasMore = false; emit searchChanged(); return;
  }
  const auto path = QStringLiteral("/search/%1?page=%2")
    .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())), QString::number(qMax(1, page)));
  get(path, [this, page](const QJsonObject &root) {
    const auto data = payload(root);
    auto items = cardList(root.value(QStringLiteral("results")));
    if (items.isEmpty()) items = cardList(data);
    if (page <= 1) m_searchResults = items; else m_searchResults.append(items);
    const auto pageInfo = data.value(QStringLiteral("pageInfo")).toObject();
    auto totalPages = firstInt(data, {"totalPages", "total_pages"});
    if (totalPages <= 0) totalPages = firstInt(pageInfo, {"totalPages", "total_pages"});
    m_searchHasMore = pageInfo.contains(QStringLiteral("hasNextPage"))
      ? pageInfo.value(QStringLiteral("hasNextPage")).toBool()
      : totalPages > 0 ? page < totalPages : !items.isEmpty();
    emit searchChanged();
  });
}

void ProviderClient::loadDetails(const QString &animeId) {
  m_details.clear(); m_episodes.clear(); m_recommendations.clear(); emit detailsChanged();
  const auto encoded = QString::fromUtf8(QUrl::toPercentEncoding(animeId));
  get(QStringLiteral("/anime/%1").arg(encoded), [this, animeId](const QJsonObject &root) {
    const auto data = payload(root);
    auto anime = data.value(QStringLiteral("anime")).toObject();
    if (anime.isEmpty()) anime = data;
    m_details = card(anime);
    for (auto it = anime.begin(); it != anime.end(); ++it) m_details.insert(it.key(), it.value().toVariant());
    auto episodeValue = data.value(QStringLiteral("episodes"));
    if (episodeValue.isUndefined()) episodeValue = anime.value(QStringLiteral("episodes"));
    if (episodeValue.isArray()) m_episodes = episodeList(episodeValue, animeId);
    emit detailsChanged();
  });
  get(QStringLiteral("/recommendations/%1").arg(encoded), [this](const QJsonObject &root) {
    m_recommendations = cardList(root.value(QStringLiteral("results")));
    if (m_recommendations.isEmpty()) m_recommendations = cardList(payload(root));
    emit detailsChanged();
  });
  loadEpisodes(animeId, 0, 12);
}

void ProviderClient::loadEpisodes(const QString &animeId, int offset, int limit) {
  const auto encoded = QString::fromUtf8(QUrl::toPercentEncoding(animeId));
  get(QStringLiteral("/episodes/%1?offset=%2&limit=%3").arg(encoded).arg(qMax(0, offset)).arg(qBound(1, limit, 100)),
      [this, animeId, offset](const QJsonObject &root) {
    auto items = episodeList(payload(root), animeId);
    if (items.isEmpty()) items = episodeList(root.value(QStringLiteral("results")), animeId);
    if (offset == 0) m_episodes = items; else m_episodes.append(items);
    emit detailsChanged();
  });
}

void ProviderClient::loadServers(const QString &episodeId) {
  const auto encoded = QString::fromUtf8(QUrl::toPercentEncoding(episodeId));
  get(QStringLiteral("/servers/%1").arg(encoded), [this](const QJsonObject &root) {
    const auto data = payload(root);
    m_subServers.clear(); m_dubServers.clear();
    for (const auto &value : data.value(QStringLiteral("sub")).toArray()) if (value.isObject()) m_subServers.append(value.toObject().toVariantMap());
    for (const auto &value : data.value(QStringLiteral("dub")).toArray()) if (value.isObject()) m_dubServers.append(value.toObject().toVariantMap());
    emit serversChanged();
  });
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
  if (selected.isEmpty()) selected = firstString(data, {"file", "url", "link"});
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
  const auto path = QStringLiteral("/episode/%1?server=%2&type=%3")
    .arg(QString::fromUtf8(QUrl::toPercentEncoding(episodeId)),
         QString::fromUtf8(QUrl::toPercentEncoding(server)),
         QString::fromUtf8(QUrl::toPercentEncoding(audioMode)));
  get(path, [this, generation, episodeId, server, audioMode](const QJsonObject &root) {
    const auto stream = streamMap(root, episodeId, server, audioMode);
    if (stream.value(QStringLiteral("mediaUrl")).toString().isEmpty()) {
      emit streamFailed(generation, QStringLiteral("This server did not return a playable stream."));
    } else {
      emit streamResolved(generation, stream);
    }
  }, [this, generation](const QString &message) { emit streamFailed(generation, message); });
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
