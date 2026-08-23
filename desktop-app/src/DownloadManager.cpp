#include "DownloadManager.h"

#include "ApiClient.h"
#include "Database.h"
#include "DownloadRetryPolicy.h"
#include "HlsTools.h"

#include <QAbstractSocket>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkInformation>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QSettings>
#include <QSet>
#include <QStorageInfo>
#include <QTimer>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>
#include <utility>

struct DownloadManager::Resource {
  QUrl url;
  QString relativePath;
  QString byteRange;
  QString kind;
};

struct DownloadManager::Job {
  QString id;
  QVariantMap record;
  QVariantMap headers;
  QString root;
  int preferredHeight = 1080;
  QList<Resource> queued;
  QHash<QNetworkReply *, Resource> active;
  QHash<QNetworkReply *, QFile *> writers;
  QHash<QString, QString> paths;
  QHash<QString, qint64> completedPaths;
  QSet<QString> completedUrls;
  QSet<QString> expectedPaths;
  QSet<QString> resolvingHosts;
  qint64 completedBytes = 0;
  qint64 totalBytes = 0;
  int completedResources = 0;
  int totalResources = 0;
  int pendingResolutions = 0;
  int pendingRetries = 0;
  int concurrencyLimit = 4;
  int successfulSinceThrottle = 0;
  quint64 retryGeneration = 0;
  QHash<QString, int> retryAttempts;
  bool paused = false;
  bool cancelled = false;
};

namespace {
// Four requests retain parallel segment downloads while avoiding the burst of
// six requests that commonly triggers provider throttling. A 429 temporarily
// reduces this per-job limit further and successful transfers restore it.
constexpr int ConcurrentResources = 4;
constexpr int MinimumConcurrentResources = 1;
constexpr int SuccessfulResourcesBeforeRecovery = 12;

QString retryKey(const QUrl &url, const QString &relativePath) {
  return url.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded)
         + QLatin1Char('|') + relativePath;
}

int retryDelayWithJitter(int attempt, const QByteArray &retryAfter) {
  const int base = DownloadRetryPolicy::delayMs(attempt, retryAfter);
  const int jitter = QRandomGenerator::global()->bounded(qMax(2, base / 5));
  return base + jitter;
}

QByteArray hashFile(const QString &path) {
  QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {};
  QCryptographicHash hash(QCryptographicHash::Sha256);
  while (!file.atEnd()) hash.addData(file.read(1024 * 1024));
  return hash.result();
}

bool needsPublicResolution(const QString &host) {
  const auto normalized = host.toLower();
  return normalized.endsWith(QStringLiteral(".tiktokcdn.com")) ||
         normalized.endsWith(QStringLiteral(".byteoversea.com")) ||
         normalized.endsWith(QStringLiteral(".ibyteimg.com"));
}

}

DownloadManager::DownloadManager(Database *database, AccountClient *account, ProviderClient *provider, QObject *parent)
  : QObject(parent), m_database(database), m_account(account), m_provider(provider) {
  QSettings settings;
  m_storageRoot = settings.value(QStringLiteral("downloads/storageRoot"), database->libraryRoot()).toString();
  QDir().mkpath(m_storageRoot);
  connect(m_provider, &ProviderClient::streamResolved, this, [this](int generation, const QVariantMap &stream) {
    if (!m_pendingEpisodes.contains(generation)) return;
    const auto episode = m_pendingEpisodes.take(generation);
    enqueue(episode, stream, episode.value(QStringLiteral("preferredHeight"), 1080).toInt());
    emit preparingChanged();
  });
  connect(m_provider, &ProviderClient::streamFailed, this, [this](int generation, const QString &message) {
    if (m_pendingEpisodes.remove(generation) > 0) { setError(message); emit preparingChanged(); }
  });
  connect(m_account, &AccountClient::authenticationChanged, this, [this] {
    if (!m_account->authenticated()) {
      const auto ids = m_jobs.keys();
      for (const auto &id : ids) pause(id);
      m_pendingEpisodes.clear();
      emit preparingChanged();
    }
    reload();
  });
  reload();
}

DownloadManager::~DownloadManager() {
  for (auto *job : std::as_const(m_jobs)) {
    for (auto *reply : job->active.keys()) {
      QObject::disconnect(reply, nullptr, this, nullptr);
      closeResourceWriter(job, reply);
      reply->abort();
    }
    delete job;
  }
}

void DownloadManager::setError(const QString &error) { if (m_error == error) return; m_error = error; emit errorChanged(); }

void DownloadManager::reload() {
  const auto owner = m_account->user().value(QStringLiteral("id")).toString();
  if (owner.isEmpty()) {
    m_items.clear(); m_groups.clear(); emit itemsChanged(); return;
  }
  auto records = m_database->downloads(owner);
  bool recoveredInterruptedJob = false;
  for (const auto &value : records) {
    const auto record = value.toMap();
    const auto id = record.value(QStringLiteral("id")).toString();
    const auto state = record.value(QStringLiteral("state")).toString();
    if (!m_jobs.contains(id) && (state == QStringLiteral("queued") || state == QStringLiteral("preparing") ||
                                state == QStringLiteral("downloading") || state == QStringLiteral("validating"))) {
      m_database->updateDownloadState(id, QStringLiteral("paused"), record.value(QStringLiteral("progress")).toDouble(),
                                      record.value(QStringLiteral("completedBytes")).toLongLong(), record.value(QStringLiteral("totalBytes")).toLongLong(),
                                      QStringLiteral("Download was paused when AniCloud closed."));
      recoveredInterruptedJob = true;
    }
  }
  m_items = recoveredInterruptedJob ? m_database->downloads(owner) : records;
  m_groups.clear();
  QHash<QString, qsizetype> groupIndexes;
  for (const auto &value : std::as_const(m_items)) {
    const auto record = value.toMap();
    auto groupKey = record.value(QStringLiteral("animeId")).toString();
    if (groupKey.isEmpty()) groupKey = record.value(QStringLiteral("animeName")).toString();
    qsizetype groupIndex = groupIndexes.value(groupKey, -1);
    if (groupIndex < 0) {
      groupIndex = m_groups.size();
      groupIndexes.insert(groupKey, groupIndex);
      m_groups.append(QVariantMap{
        {QStringLiteral("key"), groupKey},
        {QStringLiteral("animeId"), record.value(QStringLiteral("animeId"))},
        {QStringLiteral("animeName"), record.value(QStringLiteral("animeName"))},
        {QStringLiteral("animeImage"), record.value(QStringLiteral("animeImage"))},
        {QStringLiteral("episodes"), QVariantList{}},
        {QStringLiteral("episodeCount"), 0},
        {QStringLiteral("completedCount"), 0},
        {QStringLiteral("activeCount"), 0},
        {QStringLiteral("failedCount"), 0},
        {QStringLiteral("progress"), 0.0},
      });
    }
    auto group = m_groups.at(groupIndex).toMap();
    auto episodes = group.value(QStringLiteral("episodes")).toList();
    episodes.append(record);
    group.insert(QStringLiteral("episodes"), episodes);
    group.insert(QStringLiteral("episodeCount"), episodes.size());
    group.insert(QStringLiteral("progress"), group.value(QStringLiteral("progress")).toDouble() + record.value(QStringLiteral("progress")).toDouble());
    const auto state = record.value(QStringLiteral("state")).toString();
    if (state == QStringLiteral("completed"))
      group.insert(QStringLiteral("completedCount"), group.value(QStringLiteral("completedCount")).toInt() + 1);
    if (state == QStringLiteral("queued") || state == QStringLiteral("preparing") ||
        state == QStringLiteral("downloading") || state == QStringLiteral("validating"))
      group.insert(QStringLiteral("activeCount"), group.value(QStringLiteral("activeCount")).toInt() + 1);
    if (state == QStringLiteral("failed"))
      group.insert(QStringLiteral("failedCount"), group.value(QStringLiteral("failedCount")).toInt() + 1);
    if (group.value(QStringLiteral("animeImage")).toString().isEmpty() && !record.value(QStringLiteral("animeImage")).toString().isEmpty())
      group.insert(QStringLiteral("animeImage"), record.value(QStringLiteral("animeImage")));
    m_groups[groupIndex] = group;
  }
  for (qsizetype index = 0; index < m_groups.size(); ++index) {
    auto group = m_groups.at(index).toMap();
    const auto count = qMax(1, group.value(QStringLiteral("episodeCount")).toInt());
    group.insert(QStringLiteral("progress"), group.value(QStringLiteral("progress")).toDouble() / count);
    group.insert(QStringLiteral("state"), group.value(QStringLiteral("activeCount")).toInt() > 0
      ? QStringLiteral("downloading")
      : group.value(QStringLiteral("failedCount")).toInt() > 0
        ? QStringLiteral("failed")
        : group.value(QStringLiteral("completedCount")).toInt() == count
          ? QStringLiteral("completed") : QStringLiteral("paused"));
    m_groups[index] = group;
  }
  emit itemsChanged();
}

DownloadManager::Job *DownloadManager::jobFor(const QString &id) const { return m_jobs.value(id, nullptr); }

QVariantMap DownloadManager::episodeStatus(const QString &animeId, const QString &episodeId) const {
  for (auto it = m_pendingEpisodes.cbegin(); it != m_pendingEpisodes.cend(); ++it) {
    const auto pending = it.value();
    if (pending.value(QStringLiteral("animeId")).toString() == animeId &&
        pending.value(QStringLiteral("episodeId"), pending.value(QStringLiteral("id"))).toString() == episodeId) {
      auto value = pending;
      value.insert(QStringLiteral("state"), QStringLiteral("preparing"));
      value.insert(QStringLiteral("progress"), 0.0);
      return value;
    }
  }
  for (const auto &value : m_items) {
    const auto record = value.toMap();
    if (record.value(QStringLiteral("animeId")).toString() == animeId &&
        record.value(QStringLiteral("episodeId")).toString() == episodeId) return record;
  }
  return {};
}

QString DownloadManager::enqueue(const QVariantMap &episode, const QVariantMap &stream, int preferredHeight) {
  if (!m_account->authenticated()) { setError(QStringLiteral("Sign in to download episodes.")); return {}; }
  if (QNetworkInformation::instance() && QNetworkInformation::instance()->isMetered() &&
      !QSettings().value(QStringLiteral("downloads/allowMetered"), false).toBool()) {
    setError(QStringLiteral("Downloads on metered networks are disabled in Profile.")); return {};
  }
  const QUrl media(stream.value(QStringLiteral("mediaUrl")).toString());
  if (!media.isValid()) { setError(QStringLiteral("No downloadable stream is available.")); return {}; }
  auto *job = new Job;
  job->id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  job->root = m_storageRoot + QLatin1Char('/') + job->id;
  job->headers = stream.value(QStringLiteral("headers")).toMap();
  if (!stream.value(QStringLiteral("referer")).toString().isEmpty()) job->headers.insert(QStringLiteral("Referer"), stream.value(QStringLiteral("referer")));
  job->preferredHeight = qBound(144, preferredHeight, 4320);
  job->record = episode;
  job->record.insert(QStringLiteral("episodeId"),
                     episode.value(QStringLiteral("episodeId"), episode.value(QStringLiteral("id"))));
  if (!job->record.contains(QStringLiteral("episodeNumber"))) job->record.insert(QStringLiteral("episodeNumber"), episode.value(QStringLiteral("number")));
  if (!job->record.contains(QStringLiteral("episodeName"))) job->record.insert(QStringLiteral("episodeName"), episode.value(QStringLiteral("title"), QStringLiteral("Episode %1").arg(episode.value(QStringLiteral("number")).toInt())));
  job->record.insert(QStringLiteral("audioMode"), stream.value(QStringLiteral("audioMode"), episode.value(QStringLiteral("audioMode"), QStringLiteral("sub"))));
  job->record.insert(QStringLiteral("server"), stream.value(QStringLiteral("server"), episode.value(QStringLiteral("server"), QStringLiteral("hd-2"))));
  job->record.insert(QStringLiteral("id"), job->id);
  job->record.insert(QStringLiteral("ownerId"), m_account->user().value(QStringLiteral("id")).toString());
  job->record.insert(QStringLiteral("mediaUrl"), media.toString(QUrl::FullyEncoded));
  job->record.insert(QStringLiteral("headers"), job->headers);
  job->record.insert(QStringLiteral("subtitles"), stream.value(QStringLiteral("subtitles")));
  job->record.insert(QStringLiteral("introStart"), stream.value(QStringLiteral("introStart")));
  job->record.insert(QStringLiteral("introEnd"), stream.value(QStringLiteral("introEnd")));
  job->record.insert(QStringLiteral("outroStart"), stream.value(QStringLiteral("outroStart")));
  job->record.insert(QStringLiteral("outroEnd"), stream.value(QStringLiteral("outroEnd")));
  job->record.insert(QStringLiteral("referer"), stream.value(QStringLiteral("referer")));
  job->record.insert(QStringLiteral("rootPath"), job->root);
  job->record.insert(QStringLiteral("state"), QStringLiteral("queued"));
  job->record.insert(QStringLiteral("qualityHeight"), job->preferredHeight);
  job->record.insert(QStringLiteral("progress"), 0.0);
  job->record.insert(QStringLiteral("completedBytes"), 0);
  job->record.insert(QStringLiteral("totalBytes"), 0);
  if (!QDir().mkpath(job->root + QStringLiteral("/resources"))) { delete job; setError(QStringLiteral("Unable to create the download directory.")); return {}; }
  QString databaseError;
  if (!m_database->upsertDownload(job->record, &databaseError)) { QDir(job->root).removeRecursively(); delete job; setError(databaseError); return {}; }
  m_jobs.insert(job->id, job); setError({}); reload(); start(job); return job->id;
}

void DownloadManager::enqueueEpisode(const QVariantMap &episode, int preferredHeight) {
  if (!m_account->authenticated()) { setError(QStringLiteral("Sign in to download episodes.")); return; }
  const auto animeId = episode.value(QStringLiteral("animeId")).toString();
  const auto episodeId = episode.value(QStringLiteral("episodeId"), episode.value(QStringLiteral("id"))).toString();
  const auto existing = episodeStatus(animeId, episodeId);
  const auto existingState = existing.value(QStringLiteral("state")).toString();
  if (!existingState.isEmpty()) {
    if (existingState == QStringLiteral("paused") || existingState == QStringLiteral("failed") || existingState == QStringLiteral("cancelled"))
      resume(existing.value(QStringLiteral("id")).toString());
    else
      setError(existingState == QStringLiteral("completed") ? QStringLiteral("This episode is already downloaded.")
                                                              : QStringLiteral("This episode is already being downloaded."));
    return;
  }
  auto pending = episode;
  pending.insert(QStringLiteral("preferredHeight"), preferredHeight);
  if (!pending.contains(QStringLiteral("server")))
    pending.insert(QStringLiteral("server"), QSettings().value(QStringLiteral("playback/server"), QStringLiteral("hd-2")));
  const int generation = ++m_resolveGeneration;
  m_pendingEpisodes.insert(generation, pending);
  emit preparingChanged();
  m_provider->resolveStream(generation,
    episode.value(QStringLiteral("episodeId"), episode.value(QStringLiteral("id"))).toString(),
    pending.value(QStringLiteral("server"), QStringLiteral("hd-2")).toString(),
    episode.value(QStringLiteral("audioMode"), QStringLiteral("sub")).toString());
}

void DownloadManager::start(Job *job) {
  ++job->retryGeneration;
  job->queued.clear(); job->expectedPaths.clear(); job->completedUrls.clear(); job->completedPaths.clear();
  job->completedBytes = 0; job->totalBytes = 0; job->completedResources = 0; job->totalResources = 0;
  job->pendingRetries = 0; job->retryAttempts.clear(); job->concurrencyLimit = ConcurrentResources;
  job->successfulSinceThrottle = 0;
  job->record.remove(QStringLiteral("failure"));
  for (const auto &value : m_database->downloadResources(job->id)) {
    const auto resource = value.toMap();
    const QUrl remote(resource.value(QStringLiteral("url")).toString());
    const auto relative = resource.value(QStringLiteral("relativePath")).toString();
    if (remote.isValid() && !relative.isEmpty())
      job->paths.insert(remote.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded), relative);
    const QFileInfo local(job->root + QLatin1Char('/') + relative);
    const auto expectedSize = resource.value(QStringLiteral("size")).toLongLong();
    if (resource.value(QStringLiteral("completed")).toBool() && local.isFile() && local.size() > 0 &&
        (expectedSize <= 0 || expectedSize == local.size())) job->completedPaths.insert(relative, local.size());
  }
  job->paused = false; job->cancelled = false; update(job, QStringLiteral("preparing"));
  fetchManifest(job, QUrl(job->record.value(QStringLiteral("mediaUrl")).toString()), true);
}

void DownloadManager::fetchManifest(Job *job, const QUrl &url, bool selectVariant, int attempt) {
  QNetworkRequest request(url); request.setTransferTimeout(30'000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  for (auto it = job->headers.cbegin(); it != job->headers.cend(); ++it) request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
  auto *reply = m_network.get(request);
  job->active.insert(reply, {url, QStringLiteral("offline.m3u8"), {}, QStringLiteral("root-playlist")});
  connect(reply, &QNetworkReply::finished, this, [this, job, reply, selectVariant, attempt] {
    job->active.remove(reply);
    if (job->cancelled || job->paused) { reply->deleteLater(); return; }
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto networkError = reply->error();
    if (networkError != QNetworkReply::NoError || status >= 400) {
      const int nextAttempt = attempt + 1;
      if (nextAttempt <= DownloadRetryPolicy::MaxAutomaticRetries &&
          DownloadRetryPolicy::shouldRetry(status, networkError)) {
        const auto jobId = job->id;
        const auto generation = job->retryGeneration;
        const int delay = retryDelayWithJitter(nextAttempt, reply->rawHeader(QByteArrayLiteral("Retry-After")));
        ++job->pendingRetries;
        reply->deleteLater();
        QTimer::singleShot(delay, this, [this, jobId, generation, url, selectVariant, nextAttempt] {
          auto *activeJob = jobFor(jobId);
          if (!activeJob || activeJob->retryGeneration != generation) return;
          activeJob->pendingRetries = qMax(0, activeJob->pendingRetries - 1);
          if (activeJob->cancelled || activeJob->paused) return;
          fetchManifest(activeJob, url, selectVariant, nextAttempt);
        });
        return;
      }
      const auto message = networkError != QNetworkReply::NoError
        ? reply->errorString()
        : QStringLiteral("The video provider returned HTTP %1.").arg(status);
      reply->deleteLater(); fail(job, message); return;
    }
    const auto body = reply->readAll(); const auto finalUrl = reply->url(); reply->deleteLater();
    if (!HlsTools::looksLikePlaylist(body, finalUrl)) { fail(job, QStringLiteral("The source is not an HLS playlist.")); return; }
    if (selectVariant) {
      const auto available = HlsTools::variants(body, finalUrl);
      if (!available.isEmpty()) { fetchManifest(job, HlsTools::selectVariant(available, job->preferredHeight).url, false); return; }
    }
    prepareMediaManifest(job, body, finalUrl);
  });
}

QString DownloadManager::localResource(Job *job, const QUrl &url, const QString &resourceKind) {
  const auto key = url.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
  if (job->paths.contains(key)) return job->paths.value(key);
  const auto extension = HlsTools::offlineExtension(url, resourceKind);
  const auto relative = QStringLiteral("resources/%1.%2")
    .arg(QString::fromLatin1(QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex().left(24)), extension);
  job->paths.insert(key, relative);
  return relative;
}

void DownloadManager::prepareMediaManifest(Job *job, const QByteArray &body, const QUrl &url) {
  const auto listed = HlsTools::resources(body, url);
  for (const auto &entry : listed) {
    const auto relative = localResource(job, entry.url, entry.kind);
    // Download the complete backing object once. The offline manifest keeps its
    // original EXT-X-BYTERANGE offsets, which remain valid against the full file.
    queueResource(job, {entry.url, relative, {}, entry.kind});
  }
  const auto supplemental = job->record.value(QStringLiteral("subtitles")).toList();
  QVariantList storedSubtitles;
  QList<QPair<QString, QUrl>> offlineSubtitles;
  for (const auto &subtitleValue : supplemental) {
    auto subtitle = subtitleValue.toMap();
    const QUrl subtitleUrl(subtitle.value(QStringLiteral("url"), subtitle.value(QStringLiteral("file"))).toString());
    if (subtitleUrl.isValid()) {
      const auto relative = localResource(job, subtitleUrl, QStringLiteral("subtitle"));
      queueResource(job, {subtitleUrl, relative, {}, HlsTools::looksLikePlaylist({}, subtitleUrl) ? QStringLiteral("playlist") : QStringLiteral("subtitle")});
      subtitle.insert(QStringLiteral("localPath"), relative);
      storedSubtitles.append(subtitle);
      offlineSubtitles.append({subtitle.value(QStringLiteral("label"), QStringLiteral("Captions")).toString(), QUrl(relative)});
    }
  }
  job->record.insert(QStringLiteral("subtitles"), storedSubtitles);
  const QUrl artwork(job->record.value(QStringLiteral("animeImage")).toString());
  if (artwork.isValid()) {
    const auto relative = QStringLiteral("artwork.%1").arg(QFileInfo(artwork.path()).suffix().isEmpty() ? QStringLiteral("jpg") : QFileInfo(artwork.path()).suffix());
    queueResource(job, {artwork, relative, {}, QStringLiteral("artwork")});
  }
  const auto rewritten = HlsTools::rewrite(body, url, [job, this](const QUrl &remote) { return QUrl(localResource(job, remote)); });
  QSaveFile mediaFile(job->root + QStringLiteral("/media.m3u8"));
  if (!mediaFile.open(QIODevice::WriteOnly) || mediaFile.write(rewritten) != rewritten.size() || !mediaFile.commit()) { fail(job, mediaFile.errorString()); return; }
  const auto offlineMaster = HlsTools::makeOfflineMaster(QUrl(QStringLiteral("media.m3u8")), offlineSubtitles);
  QSaveFile masterFile(job->root + QStringLiteral("/offline.m3u8"));
  if (!masterFile.open(QIODevice::WriteOnly) || masterFile.write(offlineMaster) != offlineMaster.size() || !masterFile.commit()) { fail(job, masterFile.errorString()); return; }
  QString metadataError;
  if (!m_database->upsertDownload(job->record, &metadataError)) { fail(job, metadataError); return; }
  resolveDownloadHosts(job);
  update(job, job->pendingResolutions > 0 ? QStringLiteral("preparing") : QStringLiteral("downloading"));
  pump(job);
}

bool DownloadManager::queueResource(Job *job, const Resource &resource) {
  if (!resource.url.isValid() || resource.relativePath.isEmpty() || job->expectedPaths.contains(resource.relativePath)) return false;
  job->expectedPaths.insert(resource.relativePath); ++job->totalResources;
  const auto completedSize = job->completedPaths.value(resource.relativePath);
  const bool playlistResource = resource.kind == QStringLiteral("playlist") || resource.kind == QStringLiteral("media") ||
                                HlsTools::looksLikePlaylist({}, resource.url);
  if (!playlistResource && completedSize > 0) {
    ++job->completedResources; job->completedBytes += completedSize;
    job->completedUrls.insert(resource.url.toString(QUrl::FullyEncoded));
  } else {
    job->queued.append(resource);
  }
  return true;
}

void DownloadManager::pump(Job *job) {
  if (job->paused || job->cancelled || job->pendingResolutions > 0) return;
  while (job->active.size() < job->concurrencyLimit && !job->queued.isEmpty()) fetchResource(job, job->queued.takeFirst());
  if (job->active.isEmpty() && job->queued.isEmpty()) {
    if (job->pendingRetries > 0) return;
    update(job, QStringLiteral("validating"));
    if (!QFileInfo::exists(job->root + QStringLiteral("/offline.m3u8"))) { fail(job, QStringLiteral("Offline playlist validation failed.")); return; }
    for (const auto &relative : std::as_const(job->expectedPaths)) {
      const QFileInfo resource(job->root + QLatin1Char('/') + relative);
      if (!resource.isFile() || resource.size() <= 0) { fail(job, QStringLiteral("An offline resource failed integrity validation.")); return; }
    }
    update(job, QStringLiteral("completed")); emit downloadCompleted(job->id); m_jobs.remove(job->id); delete job;
  }
}

void DownloadManager::fetchResource(Job *job, const Resource &resource) {
  const auto resourceId = QString::fromLatin1(QCryptographicHash::hash((resource.url.toString(QUrl::FullyEncoded) + QLatin1Char('|') + resource.relativePath).toUtf8(), QCryptographicHash::Sha256).toHex());
  m_database->upsertDownloadResource(job->id, {
    {QStringLiteral("id"), resourceId}, {QStringLiteral("url"), resource.url.toString(QUrl::FullyEncoded)},
    {QStringLiteral("relativePath"), resource.relativePath}, {QStringLiteral("byteRange"), resource.byteRange},
  });
  const auto partPath = job->root + QLatin1Char('/') + resource.relativePath + QStringLiteral(".part");
  QDir().mkpath(QFileInfo(partPath).absolutePath());
  const bool playlistResource = resource.kind == QStringLiteral("playlist") || resource.kind == QStringLiteral("media") ||
                                HlsTools::looksLikePlaylist({}, resource.url);
  if (!resource.byteRange.isEmpty() || playlistResource) QFile::remove(partPath);
  const auto existing = QFileInfo(partPath).size();
  auto routedUrl = resource.url;
  const auto publicAddress = m_publicAddresses.value(resource.url.host());
  if (!publicAddress.isEmpty()) routedUrl.setHost(publicAddress);
  QNetworkRequest request(routedUrl); request.setTransferTimeout(45'000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  for (auto it = job->headers.cbegin(); it != job->headers.cend(); ++it) request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
  if (!publicAddress.isEmpty()) {
    request.setPeerVerifyName(resource.url.host());
    request.setRawHeader(QByteArrayLiteral("Host"), resource.url.host().toUtf8());
  }
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));
  request.setRawHeader(QByteArrayLiteral("Accept-Encoding"), QByteArrayLiteral("identity"));
  if (!resource.byteRange.isEmpty()) request.setRawHeader(QByteArrayLiteral("Range"), QByteArrayLiteral("bytes=") + resource.byteRange.toUtf8());
  else if (existing > 0) request.setRawHeader(QByteArrayLiteral("Range"), QByteArrayLiteral("bytes=") + QByteArray::number(existing) + QByteArrayLiteral("-"));
  auto *reply = m_network.get(request); job->active.insert(reply, resource);
  connect(reply, &QNetworkReply::readyRead, this, [this, job, reply] { writeResourceData(job, reply); });
  connect(reply, &QNetworkReply::finished, this, [this, job, reply] { finishResource(job, reply); });
}

void DownloadManager::retryResourceLater(Job *job, const Resource &resource, int delayMs) {
  const auto jobId = job->id;
  const auto generation = job->retryGeneration;
  ++job->pendingRetries;
  QTimer::singleShot(delayMs, this, [this, jobId, generation, resource] {
    auto *activeJob = jobFor(jobId);
    if (!activeJob || activeJob->retryGeneration != generation) return;
    activeJob->pendingRetries = qMax(0, activeJob->pendingRetries - 1);
    if (activeJob->cancelled) return;
    activeJob->queued.prepend(resource);
    if (!activeJob->paused) pump(activeJob);
  });
}

void DownloadManager::resolveDownloadHosts(Job *job) {
  QSet<QString> hosts;
  for (const auto &resource : std::as_const(job->queued)) {
    if (needsPublicResolution(resource.url.host()) && !m_publicAddresses.contains(resource.url.host()) &&
        !job->resolvingHosts.contains(resource.url.host())) hosts.insert(resource.url.host());
  }
  for (const auto &host : hosts) {
    job->resolvingHosts.insert(host); ++job->pendingResolutions;
    QUrl url(QStringLiteral("https://cloudflare-dns.com/dns-query"));
    QUrlQuery query; query.addQueryItem(QStringLiteral("name"), host); query.addQueryItem(QStringLiteral("type"), QStringLiteral("A"));
    url.setQuery(query);
    QNetworkRequest request(url); request.setTransferTimeout(8'000);
    request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/dns-json"));
    request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArrayLiteral("AniCloudDesktop/4.0"));
    auto *reply = m_network.get(request);
    const auto jobId = job->id;
    connect(reply, &QNetworkReply::finished, this, [this, reply, host, jobId] {
      QString address;
      const auto root = QJsonDocument::fromJson(reply->readAll()).object();
      if (reply->error() == QNetworkReply::NoError) {
        for (const auto &value : root.value(QStringLiteral("Answer")).toArray()) {
          const auto answer = value.toObject();
          QHostAddress candidate(answer.value(QStringLiteral("data")).toString());
          if (answer.value(QStringLiteral("type")).toInt() == 1 && candidate.protocol() == QAbstractSocket::IPv4Protocol) {
            address = candidate.toString(); break;
          }
        }
      }
      if (!address.isEmpty()) m_publicAddresses.insert(host, address);
      reply->deleteLater();
      auto *activeJob = jobFor(jobId);
      if (!activeJob) return;
      activeJob->resolvingHosts.remove(host);
      activeJob->pendingResolutions = qMax(0, activeJob->pendingResolutions - 1);
      if (activeJob->pendingResolutions == 0 && !activeJob->paused && !activeJob->cancelled) {
        update(activeJob, QStringLiteral("downloading")); pump(activeJob);
      }
    });
  }
}

void DownloadManager::writeResourceData(Job *job, QNetworkReply *reply) {
  if (!job->active.contains(reply)) return;
  // Error pages (especially HTTP 429 bodies) must never replace a valid
  // partially downloaded media segment. Keep the .part file for Range resume.
  if (reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() >= 400) {
    reply->readAll();
    return;
  }
  auto *writer = job->writers.value(reply, nullptr);
  if (!writer) {
    const auto resource = job->active.value(reply);
    const auto partPath = job->root + QLatin1Char('/') + resource.relativePath + QStringLiteral(".part");
    writer = new QFile(partPath, this);
    const bool playlistResource = resource.kind == QStringLiteral("playlist") || resource.kind == QStringLiteral("media") ||
                                  HlsTools::looksLikePlaylist({}, resource.url);
    const bool append = resource.byteRange.isEmpty() && !playlistResource &&
                        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 206 && QFileInfo::exists(partPath);
    if (!writer->open(QIODevice::WriteOnly | (append ? QIODevice::Append : QIODevice::Truncate))) {
      reply->setProperty("anicloudWriteError", writer->errorString()); delete writer; reply->abort(); return;
    }
    job->writers.insert(reply, writer);
  }
  const auto chunk = reply->readAll();
  if (!chunk.isEmpty() && writer->write(chunk) != chunk.size()) {
    reply->setProperty("anicloudWriteError", writer->errorString()); reply->abort();
  }
}

void DownloadManager::closeResourceWriter(Job *job, QNetworkReply *reply) {
  if (auto *writer = job->writers.take(reply)) {
    writer->flush(); writer->close(); delete writer;
  }
}

void DownloadManager::finishResource(Job *job, QNetworkReply *reply) {
  writeResourceData(job, reply);
  const auto resource = job->active.take(reply);
  closeResourceWriter(job, reply);
  if (job->cancelled || job->paused) { reply->deleteLater(); return; }
  const auto writeError = reply->property("anicloudWriteError").toString();
  const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  const auto networkError = reply->error();
  if (networkError != QNetworkReply::NoError || status >= 400 || !writeError.isEmpty()) {
    const auto key = retryKey(resource.url, resource.relativePath);
    const int nextAttempt = job->retryAttempts.value(key) + 1;
    if (writeError.isEmpty() && nextAttempt <= DownloadRetryPolicy::MaxAutomaticRetries &&
        DownloadRetryPolicy::shouldRetry(status, networkError)) {
      job->retryAttempts.insert(key, nextAttempt);
      if (status == 429) {
        job->concurrencyLimit = qMax(MinimumConcurrentResources, job->concurrencyLimit / 2);
        job->successfulSinceThrottle = 0;
      }
      const int delay = retryDelayWithJitter(nextAttempt, reply->rawHeader(QByteArrayLiteral("Retry-After")));
      reply->deleteLater();
      retryResourceLater(job, resource, delay);
      pump(job);
      return;
    }
    const auto message = !writeError.isEmpty() ? writeError
      : networkError != QNetworkReply::NoError ? reply->errorString()
      : QStringLiteral("The video provider returned HTTP %1.").arg(status);
    reply->deleteLater(); fail(job, message); return;
  }
  const auto finalUrl = reply->url();
  const auto finalPath = job->root + QLatin1Char('/') + resource.relativePath;
  const auto partPath = finalPath + QStringLiteral(".part");
  const QStorageInfo storage(job->root);
  if (storage.isValid() && storage.bytesAvailable() < 8 * 1024 * 1024) {
    reply->deleteLater(); fail(job, QStringLiteral("There is not enough free disk space to continue this download.")); return;
  }
  QFile completedPart(partPath);
  if (!completedPart.open(QIODevice::ReadOnly)) { reply->deleteLater(); fail(job, completedPart.errorString()); return; }
  const auto prefix = completedPart.peek(256);
  const bool isPlaylist = HlsTools::looksLikePlaylist(prefix, finalUrl);
  QByteArray body;
  if (isPlaylist) body = completedPart.readAll();
  completedPart.close();
  if (isPlaylist) {
    const auto nested = HlsTools::resources(body, finalUrl);
    for (const auto &entry : nested) {
      const auto relative = localResource(job, entry.url, entry.kind);
      queueResource(job, {entry.url, relative, {}, entry.kind});
    }
    body = HlsTools::rewrite(body, finalUrl, [this, job, finalPath](const QUrl &remote) {
      return QUrl(QDir(QFileInfo(finalPath).absolutePath()).relativeFilePath(job->root + QLatin1Char('/') + localResource(job, remote)));
    });
    QFile rewritten(partPath);
    if (!rewritten.open(QIODevice::WriteOnly | QIODevice::Truncate) || rewritten.write(body) != body.size()) {
      const auto message = rewritten.errorString(); rewritten.close(); reply->deleteLater(); fail(job, message); return;
    }
    rewritten.flush(); rewritten.close();
    resolveDownloadHosts(job);
  }
  QFile::remove(finalPath);
  if (!QFile::rename(partPath, finalPath)) { reply->deleteLater(); fail(job, QStringLiteral("Unable to complete a download resource.")); return; }
  job->completedUrls.insert(resource.url.toString(QUrl::FullyEncoded));
  job->retryAttempts.remove(retryKey(resource.url, resource.relativePath));
  if (++job->successfulSinceThrottle >= SuccessfulResourcesBeforeRecovery) {
    job->successfulSinceThrottle = 0;
    job->concurrencyLimit = qMin(ConcurrentResources, job->concurrencyLimit + 1);
  }
  const auto resourceId = QString::fromLatin1(QCryptographicHash::hash((resource.url.toString(QUrl::FullyEncoded) + QLatin1Char('|') + resource.relativePath).toUtf8(), QCryptographicHash::Sha256).toHex());
  m_database->markDownloadResourceCompleted(job->id, resourceId, QFileInfo(finalPath).size());
  job->completedBytes += QFileInfo(finalPath).size(); ++job->completedResources; reply->deleteLater(); update(job); pump(job);
}

void DownloadManager::update(Job *job, const QString &state) {
  if (!state.isEmpty()) job->record.insert(QStringLiteral("state"), state);
  const double progress = job->totalResources > 0 ? static_cast<double>(job->completedResources) / job->totalResources : 0.0;
  job->record.insert(QStringLiteral("progress"), progress);
  job->record.insert(QStringLiteral("completedBytes"), job->completedBytes);
  job->record.insert(QStringLiteral("totalBytes"), job->totalBytes);
  m_database->updateDownloadState(job->id, job->record.value(QStringLiteral("state")).toString(), progress, job->completedBytes, job->totalBytes,
                                  job->record.value(QStringLiteral("failure")).toString());
  reload();
}

void DownloadManager::fail(Job *job, const QString &message) {
  job->paused = true;
  ++job->retryGeneration;
  job->pendingRetries = 0;
  for (auto *reply : job->active.keys()) {
    QObject::disconnect(reply, nullptr, this, nullptr); writeResourceData(job, reply); closeResourceWriter(job, reply); reply->abort(); reply->deleteLater();
  }
  job->active.clear();
  job->record.insert(QStringLiteral("failure"), message); setError(message); update(job, QStringLiteral("failed"));
}

void DownloadManager::pause(const QString &id) {
  auto *job = jobFor(id); if (!job) return; job->paused = true;
  for (auto it = job->active.cbegin(); it != job->active.cend(); ++it) job->queued.prepend(it.value());
  for (auto *reply : job->active.keys()) { writeResourceData(job, reply); closeResourceWriter(job, reply); reply->abort(); }
  job->active.clear(); update(job, QStringLiteral("paused"));
}

void DownloadManager::resume(const QString &id) {
  if (auto *job = jobFor(id)) {
    if (job->record.value(QStringLiteral("state")).toString() == QStringLiteral("failed") || job->totalResources == 0) {
      job->queued.clear(); job->completedResources = 0; job->totalResources = 0; start(job);
    } else { job->paused = false; update(job, QStringLiteral("downloading")); pump(job); }
    return;
  }
  retry(id);
}

void DownloadManager::retry(const QString &id) {
  for (const auto &value : m_database->downloads(m_account->user().value(QStringLiteral("id")).toString())) {
    const auto record = value.toMap(); if (record.value(QStringLiteral("id")).toString() != id) continue;
    auto *job = new Job; job->id = id; job->record = record; job->root = record.value(QStringLiteral("rootPath")).toString();
    job->headers = record.value(QStringLiteral("headers")).toMap(); job->preferredHeight = record.value(QStringLiteral("qualityHeight"), 1080).toInt();
    m_jobs.insert(id, job); start(job); return;
  }
}

void DownloadManager::cancel(const QString &id) {
  auto *job = jobFor(id); if (!job) return; job->cancelled = true;
  for (auto *reply : job->active.keys()) { QObject::disconnect(reply, nullptr, this, nullptr); writeResourceData(job, reply); closeResourceWriter(job, reply); reply->abort(); reply->deleteLater(); }
  job->active.clear(); update(job, QStringLiteral("cancelled")); m_jobs.remove(id); delete job;
}

void DownloadManager::remove(const QString &id) {
  if (jobFor(id)) cancel(id);
  QString root;
  for (const auto &entry : m_database->downloads(m_account->user().value(QStringLiteral("id")).toString())) if (entry.toMap().value(QStringLiteral("id")).toString() == id) root = entry.toMap().value(QStringLiteral("rootPath")).toString();
  if (root.isEmpty()) { setError(QStringLiteral("That download does not belong to the signed-in account.")); return; }
  if (QFileInfo(root).absoluteFilePath().startsWith(QFileInfo(m_storageRoot).absoluteFilePath() + QDir::separator())) QDir(root).removeRecursively();
  m_database->removeDownload(id); reload();
}

bool DownloadManager::copyAndVerify(const QString &source, const QString &destination, QString *error) {
  QDir sourceDir(source); if (!sourceDir.exists()) return QDir().mkpath(destination);
  if (!QDir().mkpath(destination)) { if (error) *error = QStringLiteral("Unable to create the new storage directory."); return false; }
  for (const auto &info : sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries)) {
    const auto target = destination + QLatin1Char('/') + info.fileName();
    if (info.isDir()) { if (!copyAndVerify(info.absoluteFilePath(), target, error)) return false; }
    else {
      QFile::remove(target);
      if (!QFile::copy(info.absoluteFilePath(), target) || QFileInfo(target).size() != info.size() || hashFile(target) != hashFile(info.absoluteFilePath())) {
        if (error) *error = QStringLiteral("A downloaded file failed verification while moving storage."); return false;
      }
    }
  }
  return true;
}

void DownloadManager::moveStorage(const QString &directory) {
  if (m_movingStorage || directory.isEmpty()) return;
  if (!m_jobs.isEmpty()) { setError(QStringLiteral("Pause or finish active downloads before moving the library.")); return; }
  const QUrl selected(directory);
  const auto localDirectory = selected.isLocalFile() ? selected.toLocalFile() : directory;
  const auto destination = QDir(localDirectory).absoluteFilePath(QStringLiteral("AniCloud-library"));
  if (QFileInfo(destination).absoluteFilePath() == QFileInfo(m_storageRoot).absoluteFilePath()) return;
  if (QFileInfo::exists(destination)) { setError(QStringLiteral("The selected folder already contains an AniCloud library.")); return; }
  m_movingStorage = true; emit movingStorageChanged();
  const auto source = m_storageRoot; const auto staging = destination + QStringLiteral(".staging");
  auto *watcher = new QFutureWatcher<QPair<bool, QString>>(this);
  connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, source, destination, staging] {
    const auto result = watcher->result(); watcher->deleteLater();
    if (result.first) {
      if (QDir().rename(staging, destination)) {
        QString dbError;
        if (m_database->moveDownloadRoots(source, destination, &dbError)) {
          QSettings().setValue(QStringLiteral("downloads/storageRoot"), destination); m_storageRoot = destination; emit storageRootChanged();
          QDir(source).removeRecursively();
        } else { QDir(destination).removeRecursively(); setError(dbError); }
      } else { QDir(staging).removeRecursively(); setError(QStringLiteral("Unable to activate the new storage library.")); }
    } else { QDir(staging).removeRecursively(); setError(result.second); }
    m_movingStorage = false; emit movingStorageChanged(); reload();
  });
  watcher->setFuture(QtConcurrent::run([source, staging] {
    QDir(staging).removeRecursively(); QString error; const bool ok = copyAndVerify(source, staging, &error); return qMakePair(ok, error);
  }));
}
