#pragma once

#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

class AccountClient;
class Database;
class ProviderClient;
class QNetworkReply;

class DownloadManager final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList items READ items NOTIFY itemsChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QString storageRoot READ storageRoot NOTIFY storageRootChanged)
  Q_PROPERTY(bool movingStorage READ movingStorage NOTIFY movingStorageChanged)

public:
  DownloadManager(Database *database, AccountClient *account, ProviderClient *provider, QObject *parent = nullptr);
  ~DownloadManager() override;

  [[nodiscard]] QVariantList items() const { return m_items; }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] QString storageRoot() const { return m_storageRoot; }
  [[nodiscard]] bool movingStorage() const { return m_movingStorage; }

  Q_INVOKABLE void reload();
  Q_INVOKABLE QString enqueue(const QVariantMap &episode, const QVariantMap &stream, int preferredHeight = 1080);
  Q_INVOKABLE void enqueueEpisode(const QVariantMap &episode, int preferredHeight = 1080);
  Q_INVOKABLE void pause(const QString &id);
  Q_INVOKABLE void resume(const QString &id);
  Q_INVOKABLE void retry(const QString &id);
  Q_INVOKABLE void cancel(const QString &id);
  Q_INVOKABLE void remove(const QString &id);
  Q_INVOKABLE void moveStorage(const QString &directory);

signals:
  void itemsChanged();
  void errorChanged();
  void storageRootChanged();
  void movingStorageChanged();
  void downloadCompleted(const QString &id);

private:
  struct Resource;
  struct Job;
  void start(Job *job);
  void fetchManifest(Job *job, const QUrl &url, bool selectVariant);
  void prepareMediaManifest(Job *job, const QByteArray &body, const QUrl &url);
  QString localResource(Job *job, const QUrl &url);
  bool queueResource(Job *job, const Resource &resource);
  void pump(Job *job);
  void fetchResource(Job *job, const Resource &resource);
  void resolveDownloadHosts(Job *job);
  void writeResourceData(Job *job, QNetworkReply *reply);
  void closeResourceWriter(Job *job, QNetworkReply *reply);
  void finishResource(Job *job, QNetworkReply *reply);
  void update(Job *job, const QString &state = {});
  void fail(Job *job, const QString &message);
  Job *jobFor(const QString &id) const;
  void setError(const QString &error);
  static bool copyAndVerify(const QString &source, const QString &destination, QString *error);

  Database *m_database = nullptr;
  AccountClient *m_account = nullptr;
  ProviderClient *m_provider = nullptr;
  QNetworkAccessManager m_network;
  QHash<QString, QString> m_publicAddresses;
  QHash<QString, Job *> m_jobs;
  QVariantList m_items;
  QString m_error;
  QString m_storageRoot;
  bool m_movingStorage = false;
  int m_resolveGeneration = 100000;
  QHash<int, QVariantMap> m_pendingEpisodes;
};
