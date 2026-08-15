#include "Database.h"

#include "SecureStore.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

Database::Database(SecureStore *secureStore, const QString &dataRoot)
  : m_secureStore(secureStore),
    m_connectionName(QStringLiteral("anicloud-native-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces))) {
  m_dataRoot = dataRoot.isEmpty()
    ? QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/native-v1")
    : dataRoot;
}

Database::~Database() {
  if (m_database.isValid()) m_database.close();
  const auto name = m_connectionName;
  m_database = {};
  QSqlDatabase::removeDatabase(name);
}

bool Database::open(QString *error) {
  if (!QDir().mkpath(m_dataRoot) || !QDir().mkpath(libraryRoot())) {
    if (error) *error = QStringLiteral("Unable to create AniCloud's native data directory");
    return false;
  }
  m_database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
  m_database.setDatabaseName(m_dataRoot + QStringLiteral("/anicloud-native.db"));
  if (!m_database.open()) {
    if (error) *error = m_database.lastError().text();
    return false;
  }
  return migrate(error);
}

QString Database::libraryRoot() const {
  return m_dataRoot + QStringLiteral("/library");
}

bool Database::exec(const QString &sql, QString *error) const {
  QSqlQuery query(m_database);
  if (query.exec(sql)) return true;
  if (error) *error = query.lastError().text();
  return false;
}

bool Database::migrate(QString *error) {
  if (!exec(QStringLiteral("PRAGMA journal_mode=WAL"), error) ||
      !exec(QStringLiteral("PRAGMA foreign_keys=ON"), error) ||
      !exec(QStringLiteral("CREATE TABLE IF NOT EXISTS schema_info(version INTEGER NOT NULL)"), error)) return false;

  QSqlQuery versionQuery(m_database);
  if (!versionQuery.exec(QStringLiteral("SELECT version FROM schema_info LIMIT 1"))) {
    if (error) *error = versionQuery.lastError().text();
    return false;
  }
  if (!versionQuery.next()) {
    if (!exec(QStringLiteral("INSERT INTO schema_info(version) VALUES(1)"), error)) return false;
  }

  const QStringList migrations = {
    QStringLiteral("CREATE TABLE IF NOT EXISTS local_history("
                   "anime_id TEXT NOT NULL, episode_id TEXT NOT NULL, anime_name TEXT NOT NULL, anime_image TEXT NOT NULL DEFAULT '',"
                   "episode_number INTEGER NOT NULL DEFAULT 0, episode_name TEXT NOT NULL, audio_mode TEXT NOT NULL DEFAULT 'sub',"
                   "server TEXT NOT NULL DEFAULT 'hd-1', position_seconds INTEGER NOT NULL DEFAULT 0, duration_seconds INTEGER,"
                   "episode_count INTEGER NOT NULL DEFAULT 0, watched_at INTEGER NOT NULL, PRIMARY KEY(anime_id, episode_id))"),
    QStringLiteral("CREATE TABLE IF NOT EXISTS downloads("
                   "id TEXT PRIMARY KEY, owner_id TEXT NOT NULL, anime_id TEXT NOT NULL, anime_name TEXT NOT NULL, anime_image TEXT NOT NULL DEFAULT '',"
                   "episode_id TEXT NOT NULL, episode_name TEXT NOT NULL, episode_number INTEGER NOT NULL, audio_mode TEXT NOT NULL,"
                   "quality_height INTEGER NOT NULL DEFAULT 0, server TEXT NOT NULL, state TEXT NOT NULL, progress REAL NOT NULL DEFAULT 0,"
                   "completed_bytes INTEGER NOT NULL DEFAULT 0, total_bytes INTEGER NOT NULL DEFAULT 0, root_path TEXT NOT NULL,"
                   "sensitive BLOB, failure TEXT, created_at INTEGER NOT NULL, updated_at INTEGER NOT NULL)"),
    QStringLiteral("CREATE TABLE IF NOT EXISTS download_resources("
                   "download_id TEXT NOT NULL REFERENCES downloads(id) ON DELETE CASCADE, resource_id TEXT NOT NULL, remote_url BLOB NOT NULL,"
                   "relative_path TEXT NOT NULL, byte_range TEXT, completed INTEGER NOT NULL DEFAULT 0, size INTEGER NOT NULL DEFAULT 0,"
                   "PRIMARY KEY(download_id, resource_id))"),
    QStringLiteral("CREATE TABLE IF NOT EXISTS pending_progress("
                   "key TEXT PRIMARY KEY, owner_id TEXT NOT NULL, payload BLOB NOT NULL, updated_at INTEGER NOT NULL)"),
    QStringLiteral("CREATE TABLE IF NOT EXISTS broadcasts("
                   "id TEXT PRIMARY KEY, title TEXT NOT NULL, message TEXT NOT NULL, link_url TEXT, received_at INTEGER NOT NULL, read INTEGER NOT NULL DEFAULT 0)"),
  };
  for (const auto &sql : migrations) {
    if (!exec(sql, error)) return false;
  }
  return true;
}

QByteArray Database::protectJson(const QVariantMap &value) const {
  const auto json = QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact);
  return m_secureStore ? m_secureStore->seal(json) : QByteArray{};
}

QVariantMap Database::unprotectJson(const QByteArray &value) const {
  if (!m_secureStore || value.isEmpty()) return {};
  const auto plain = m_secureStore->open(value);
  return QJsonDocument::fromJson(plain).object().toVariantMap();
}

QVariantList Database::localHistory(int limit) const {
  QVariantList result;
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("SELECT anime_id,anime_name,anime_image,episode_id,episode_number,episode_name,audio_mode,server,position_seconds,duration_seconds,episode_count,watched_at FROM local_history ORDER BY watched_at DESC LIMIT ?"));
  query.addBindValue(qBound(1, limit, 500));
  if (!query.exec()) return result;
  while (query.next()) {
    result.append(QVariantMap{
      {QStringLiteral("animeId"), query.value(0)}, {QStringLiteral("animeName"), query.value(1)},
      {QStringLiteral("animeImage"), query.value(2)}, {QStringLiteral("episodeId"), query.value(3)},
      {QStringLiteral("episodeNumber"), query.value(4)}, {QStringLiteral("episodeName"), query.value(5)},
      {QStringLiteral("audioMode"), query.value(6)}, {QStringLiteral("server"), query.value(7)},
      {QStringLiteral("positionSeconds"), query.value(8)}, {QStringLiteral("durationSeconds"), query.value(9)},
      {QStringLiteral("episodeCount"), query.value(10)}, {QStringLiteral("watchedAt"), query.value(11)},
    });
  }
  return result;
}

bool Database::saveLocalProgress(const QVariantMap &r, QString *error) {
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO local_history(anime_id,episode_id,anime_name,anime_image,episode_number,episode_name,audio_mode,server,position_seconds,duration_seconds,episode_count,watched_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(anime_id,episode_id) DO UPDATE SET anime_name=excluded.anime_name,anime_image=excluded.anime_image,episode_number=excluded.episode_number,episode_name=excluded.episode_name,audio_mode=excluded.audio_mode,server=excluded.server,position_seconds=excluded.position_seconds,duration_seconds=excluded.duration_seconds,episode_count=excluded.episode_count,watched_at=excluded.watched_at"));
  query.addBindValue(r.value(QStringLiteral("animeId"))); query.addBindValue(r.value(QStringLiteral("episodeId")));
  query.addBindValue(r.value(QStringLiteral("animeName"))); query.addBindValue(r.value(QStringLiteral("animeImage")));
  query.addBindValue(r.value(QStringLiteral("episodeNumber"))); query.addBindValue(r.value(QStringLiteral("episodeName")));
  query.addBindValue(r.value(QStringLiteral("audioMode"), QStringLiteral("sub"))); query.addBindValue(r.value(QStringLiteral("server"), QStringLiteral("hd-1")));
  query.addBindValue(r.value(QStringLiteral("positionSeconds"))); query.addBindValue(r.value(QStringLiteral("durationSeconds")));
  query.addBindValue(r.value(QStringLiteral("episodeCount"))); query.addBindValue(QDateTime::currentMSecsSinceEpoch());
  if (query.exec()) return true;
  if (error) *error = query.lastError().text();
  return false;
}

bool Database::removeLocalHistory(const QString &animeId, const QString &episodeId) {
  QSqlQuery query(m_database);
  if (episodeId.isEmpty()) {
    query.prepare(QStringLiteral("DELETE FROM local_history WHERE anime_id=?"));
    query.addBindValue(animeId);
  } else {
    query.prepare(QStringLiteral("DELETE FROM local_history WHERE anime_id=? AND episode_id=?"));
    query.addBindValue(animeId);
    query.addBindValue(episodeId);
  }
  return query.exec();
}

QVariantList Database::downloads(const QString &ownerId) const {
  QVariantList result;
  QSqlQuery query(m_database);
  if (ownerId.isEmpty()) {
    query.prepare(QStringLiteral("SELECT id,owner_id,anime_id,anime_name,anime_image,episode_id,episode_name,episode_number,audio_mode,quality_height,server,state,progress,completed_bytes,total_bytes,root_path,failure,sensitive FROM downloads ORDER BY updated_at DESC"));
  } else {
    query.prepare(QStringLiteral("SELECT id,owner_id,anime_id,anime_name,anime_image,episode_id,episode_name,episode_number,audio_mode,quality_height,server,state,progress,completed_bytes,total_bytes,root_path,failure,sensitive FROM downloads WHERE owner_id=? ORDER BY updated_at DESC"));
    query.addBindValue(ownerId);
  }
  if (!query.exec()) return result;
  while (query.next()) {
    QVariantMap value{
      {QStringLiteral("id"), query.value(0)}, {QStringLiteral("ownerId"), query.value(1)},
      {QStringLiteral("animeId"), query.value(2)}, {QStringLiteral("animeName"), query.value(3)},
      {QStringLiteral("animeImage"), query.value(4)}, {QStringLiteral("episodeId"), query.value(5)},
      {QStringLiteral("episodeName"), query.value(6)}, {QStringLiteral("episodeNumber"), query.value(7)},
      {QStringLiteral("audioMode"), query.value(8)}, {QStringLiteral("qualityHeight"), query.value(9)},
      {QStringLiteral("server"), query.value(10)}, {QStringLiteral("state"), query.value(11)},
      {QStringLiteral("progress"), query.value(12)}, {QStringLiteral("completedBytes"), query.value(13)},
      {QStringLiteral("totalBytes"), query.value(14)}, {QStringLiteral("rootPath"), query.value(15)},
      {QStringLiteral("failure"), query.value(16)},
    };
    const auto sensitive = unprotectJson(query.value(17).toByteArray());
    for (auto it = sensitive.cbegin(); it != sensitive.cend(); ++it) value.insert(it.key(), it.value());
    result.append(value);
  }
  return result;
}

bool Database::upsertDownload(const QVariantMap &r, QString *error) {
  const auto now = QDateTime::currentMSecsSinceEpoch();
  const auto sensitive = protectJson({
    {QStringLiteral("mediaUrl"), r.value(QStringLiteral("mediaUrl"))},
    {QStringLiteral("referer"), r.value(QStringLiteral("referer"))},
    {QStringLiteral("headers"), r.value(QStringLiteral("headers"))},
    {QStringLiteral("subtitles"), r.value(QStringLiteral("subtitles"))},
  });
  if (sensitive.isEmpty()) {
    if (error) *error = QStringLiteral("Secure storage is unavailable; download metadata was not persisted");
    return false;
  }
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO downloads(id,owner_id,anime_id,anime_name,anime_image,episode_id,episode_name,episode_number,audio_mode,quality_height,server,state,progress,completed_bytes,total_bytes,root_path,sensitive,failure,created_at,updated_at) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET state=excluded.state,progress=excluded.progress,completed_bytes=excluded.completed_bytes,total_bytes=excluded.total_bytes,root_path=excluded.root_path,sensitive=excluded.sensitive,failure=excluded.failure,updated_at=excluded.updated_at"));
  const QStringList keys = {QStringLiteral("id"),QStringLiteral("ownerId"),QStringLiteral("animeId"),QStringLiteral("animeName"),QStringLiteral("animeImage"),QStringLiteral("episodeId"),QStringLiteral("episodeName"),QStringLiteral("episodeNumber"),QStringLiteral("audioMode"),QStringLiteral("qualityHeight"),QStringLiteral("server"),QStringLiteral("state"),QStringLiteral("progress"),QStringLiteral("completedBytes"),QStringLiteral("totalBytes"),QStringLiteral("rootPath")};
  for (const auto &key : keys) query.addBindValue(r.value(key));
  query.addBindValue(sensitive); query.addBindValue(r.value(QStringLiteral("failure"))); query.addBindValue(now); query.addBindValue(now);
  if (query.exec()) return true;
  if (error) *error = query.lastError().text();
  return false;
}

bool Database::updateDownloadState(const QString &id, const QString &state, double progress, qint64 completedBytes, qint64 totalBytes, const QString &failure) {
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("UPDATE downloads SET state=?,progress=?,completed_bytes=?,total_bytes=?,failure=?,updated_at=? WHERE id=?"));
  query.addBindValue(state); query.addBindValue(progress); query.addBindValue(completedBytes); query.addBindValue(totalBytes);
  query.addBindValue(failure); query.addBindValue(QDateTime::currentMSecsSinceEpoch()); query.addBindValue(id);
  return query.exec();
}

bool Database::removeDownload(const QString &id, QString *error) {
  QSqlQuery query(m_database); query.prepare(QStringLiteral("DELETE FROM downloads WHERE id=?")); query.addBindValue(id);
  if (query.exec()) return true; if (error) *error = query.lastError().text(); return false;
}

bool Database::moveDownloadRoots(const QString &oldRoot, const QString &newRoot, QString *error) {
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("UPDATE downloads SET root_path=? || substr(root_path, length(?) + 1), updated_at=? WHERE root_path LIKE ?"));
  query.addBindValue(newRoot);
  query.addBindValue(oldRoot);
  query.addBindValue(QDateTime::currentMSecsSinceEpoch());
  query.addBindValue(oldRoot + QStringLiteral("/%"));
  if (query.exec()) return true;
  if (error) *error = query.lastError().text();
  return false;
}

bool Database::upsertDownloadResource(const QString &downloadId, const QVariantMap &resource) {
  const auto protectedUrl = protectJson({{QStringLiteral("url"), resource.value(QStringLiteral("url"))}});
  if (protectedUrl.isEmpty()) return false;
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO download_resources(download_id,resource_id,remote_url,relative_path,byte_range,completed,size) VALUES(?,?,?,?,?,0,0) ON CONFLICT(download_id,resource_id) DO UPDATE SET remote_url=excluded.remote_url,relative_path=excluded.relative_path,byte_range=excluded.byte_range"));
  query.addBindValue(downloadId);
  query.addBindValue(resource.value(QStringLiteral("id")));
  query.addBindValue(protectedUrl);
  query.addBindValue(resource.value(QStringLiteral("relativePath")));
  query.addBindValue(resource.value(QStringLiteral("byteRange")));
  return query.exec();
}

bool Database::markDownloadResourceCompleted(const QString &downloadId, const QString &resourceId, qint64 size) {
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("UPDATE download_resources SET completed=1,size=? WHERE download_id=? AND resource_id=?"));
  query.addBindValue(size); query.addBindValue(downloadId); query.addBindValue(resourceId);
  return query.exec();
}

QVariantList Database::downloadResources(const QString &downloadId) const {
  QVariantList result;
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("SELECT resource_id,remote_url,relative_path,byte_range,completed,size FROM download_resources WHERE download_id=? ORDER BY relative_path"));
  query.addBindValue(downloadId);
  if (!query.exec()) return result;
  while (query.next()) {
    const auto secret = unprotectJson(query.value(1).toByteArray());
    result.append(QVariantMap{
      {QStringLiteral("id"), query.value(0)}, {QStringLiteral("url"), secret.value(QStringLiteral("url"))},
      {QStringLiteral("relativePath"), query.value(2)}, {QStringLiteral("byteRange"), query.value(3)},
      {QStringLiteral("completed"), query.value(4)}, {QStringLiteral("size"), query.value(5)},
    });
  }
  return result;
}

bool Database::queueProgress(const QString &key, const QString &ownerId, const QVariantMap &payload) {
  const auto encrypted = protectJson(payload); if (encrypted.isEmpty()) return false;
  QSqlQuery query(m_database);
  query.prepare(QStringLiteral("INSERT INTO pending_progress(key,owner_id,payload,updated_at) VALUES(?,?,?,?) ON CONFLICT(key) DO UPDATE SET payload=excluded.payload,updated_at=excluded.updated_at"));
  query.addBindValue(key); query.addBindValue(ownerId); query.addBindValue(encrypted); query.addBindValue(QDateTime::currentMSecsSinceEpoch());
  return query.exec();
}

QVariantList Database::pendingProgress(const QString &ownerId) const {
  QVariantList result; QSqlQuery query(m_database);
  query.prepare(QStringLiteral("SELECT key,payload FROM pending_progress WHERE owner_id=? ORDER BY updated_at")); query.addBindValue(ownerId);
  if (!query.exec()) return result;
  while (query.next()) {
    result.append(QVariantMap{
      {QStringLiteral("key"), query.value(0)},
      {QStringLiteral("payload"), unprotectJson(query.value(1).toByteArray())},
    });
  }
  return result;
}

bool Database::removePendingProgress(const QString &key) {
  QSqlQuery query(m_database); query.prepare(QStringLiteral("DELETE FROM pending_progress WHERE key=?")); query.addBindValue(key); return query.exec();
}

bool Database::replaceBroadcasts(const QVariantList &items) {
  if (!m_database.transaction()) return false;
  for (const auto &entry : items) {
    const auto item = entry.toMap(); QSqlQuery query(m_database);
    query.prepare(QStringLiteral("INSERT INTO broadcasts(id,title,message,link_url,received_at,read) VALUES(?,?,?,?,?,COALESCE((SELECT read FROM broadcasts WHERE id=?),0)) ON CONFLICT(id) DO UPDATE SET title=excluded.title,message=excluded.message,link_url=excluded.link_url"));
    query.addBindValue(item.value(QStringLiteral("id"))); query.addBindValue(item.value(QStringLiteral("title")));
    query.addBindValue(item.value(QStringLiteral("message"))); query.addBindValue(item.value(QStringLiteral("linkUrl")));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch()); query.addBindValue(item.value(QStringLiteral("id")));
    if (!query.exec()) { m_database.rollback(); return false; }
  }
  return m_database.commit();
}

QVariantList Database::broadcasts() const {
  QVariantList result; QSqlQuery query(QStringLiteral("SELECT id,title,message,link_url,received_at,read FROM broadcasts ORDER BY received_at DESC"), m_database);
  while (query.next()) result.append(QVariantMap{{QStringLiteral("id"),query.value(0)},{QStringLiteral("title"),query.value(1)},{QStringLiteral("message"),query.value(2)},{QStringLiteral("linkUrl"),query.value(3)},{QStringLiteral("receivedAt"),query.value(4)},{QStringLiteral("read"),query.value(5)}});
  return result;
}

bool Database::markBroadcastRead(const QString &id) {
  QSqlQuery query(m_database); query.prepare(QStringLiteral("UPDATE broadcasts SET read=1 WHERE id=?")); query.addBindValue(id); return query.exec();
}
