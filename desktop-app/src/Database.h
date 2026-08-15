#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class SecureStore;

class Database final {
public:
  explicit Database(SecureStore *secureStore, const QString &dataRoot = {});
  ~Database();

  bool open(QString *error = nullptr);
  [[nodiscard]] QString dataRoot() const { return m_dataRoot; }
  [[nodiscard]] QString libraryRoot() const;

  [[nodiscard]] QVariantList localHistory(int limit = 200) const;
  bool saveLocalProgress(const QVariantMap &record, QString *error = nullptr);
  bool removeLocalHistory(const QString &animeId, const QString &episodeId = {});

  [[nodiscard]] QVariantList downloads(const QString &ownerId = {}) const;
  bool upsertDownload(const QVariantMap &record, QString *error = nullptr);
  bool updateDownloadState(const QString &id, const QString &state, double progress,
                           qint64 completedBytes, qint64 totalBytes, const QString &failure = {});
  bool removeDownload(const QString &id, QString *error = nullptr);
  bool moveDownloadRoots(const QString &oldRoot, const QString &newRoot, QString *error = nullptr);
  bool upsertDownloadResource(const QString &downloadId, const QVariantMap &resource);
  bool markDownloadResourceCompleted(const QString &downloadId, const QString &resourceId, qint64 size);
  [[nodiscard]] QVariantList downloadResources(const QString &downloadId) const;

  bool queueProgress(const QString &key, const QString &ownerId, const QVariantMap &payload);
  [[nodiscard]] QVariantList pendingProgress(const QString &ownerId) const;
  bool removePendingProgress(const QString &key);

  bool replaceBroadcasts(const QVariantList &items);
  [[nodiscard]] QVariantList broadcasts() const;
  bool markBroadcastRead(const QString &id);

private:
  bool migrate(QString *error);
  bool exec(const QString &sql, QString *error = nullptr) const;
  [[nodiscard]] QByteArray protectJson(const QVariantMap &value) const;
  [[nodiscard]] QVariantMap unprotectJson(const QByteArray &value) const;

  SecureStore *m_secureStore = nullptr;
  QString m_connectionName;
  QString m_dataRoot;
  QSqlDatabase m_database;
};
