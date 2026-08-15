#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class Database;
class SecureStore;

class ProviderClient final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QVariantList spotlight READ spotlight NOTIFY homeChanged)
  Q_PROPERTY(QVariantList recent READ recent NOTIFY homeChanged)
  Q_PROPERTY(QVariantList popular READ popular NOTIFY homeChanged)
  Q_PROPERTY(QVariantList airing READ airing NOTIFY homeChanged)
  Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
  Q_PROPERTY(QVariantMap details READ details NOTIFY detailsChanged)
  Q_PROPERTY(QVariantList episodes READ episodes NOTIFY detailsChanged)
  Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY detailsChanged)
  Q_PROPERTY(QVariantList subServers READ subServers NOTIFY serversChanged)
  Q_PROPERTY(QVariantList dubServers READ dubServers NOTIFY serversChanged)
  Q_PROPERTY(bool searchHasMore READ searchHasMore NOTIFY searchChanged)

public:
  explicit ProviderClient(QObject *parent = nullptr);

  [[nodiscard]] bool loading() const { return m_pending > 0; }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] QVariantList spotlight() const { return m_spotlight; }
  [[nodiscard]] QVariantList recent() const { return m_recent; }
  [[nodiscard]] QVariantList popular() const { return m_popular; }
  [[nodiscard]] QVariantList airing() const { return m_airing; }
  [[nodiscard]] QVariantList searchResults() const { return m_searchResults; }
  [[nodiscard]] QVariantMap details() const { return m_details; }
  [[nodiscard]] QVariantList episodes() const { return m_episodes; }
  [[nodiscard]] QVariantList recommendations() const { return m_recommendations; }
  [[nodiscard]] QVariantList subServers() const { return m_subServers; }
  [[nodiscard]] QVariantList dubServers() const { return m_dubServers; }
  [[nodiscard]] bool searchHasMore() const { return m_searchHasMore; }

  Q_INVOKABLE void loadHome();
  Q_INVOKABLE void loadCategory(const QString &kind, int page = 1);
  Q_INVOKABLE void search(const QString &query, int page = 1);
  Q_INVOKABLE void loadDetails(const QString &animeId);
  Q_INVOKABLE void loadEpisodes(const QString &animeId, int offset = 0, int limit = 12);
  Q_INVOKABLE void loadServers(const QString &episodeId);
  Q_INVOKABLE void resolveStream(int generation, const QString &episodeId,
                                  const QString &server = QStringLiteral("hd-1"),
                                  const QString &audioMode = QStringLiteral("sub"));

  // Pure DTO mapping seams used by native unit tests.
  static QVariantList cardList(const QJsonValue &value);
  static QVariantList episodeList(const QJsonValue &value, const QString &animeId);
  static QVariantMap streamMap(const QJsonObject &root, const QString &episodeId,
                               const QString &server, const QString &audioMode);
  static QVariantList parseTopAnimeHtml(const QString &html, int limit = 20);
  static QVariantList parseSeasonAnimeHtml(const QString &html, int page = 1, int limit = 20);
  static QVariantList parseSearchHtml(const QString &html, int limit = 20);
  static QVariantMap parseAnimeDetailsHtml(const QString &html, const QString &animeId,
                                           QVariantList *recommendations = nullptr);

signals:
  void loadingChanged();
  void errorChanged();
  void homeChanged();
  void searchChanged();
  void detailsChanged();
  void serversChanged();
  void streamResolved(int generation, const QVariantMap &stream);
  void streamFailed(int generation, const QString &message);

private:
  using TextSuccess = std::function<void(const QByteArray &, const QUrl &)>;
  void getText(const QUrl &url, const QList<QPair<QByteArray, QByteArray>> &headers,
               TextSuccess success, std::function<void(const QString &)> failure = {});
  void resolveStreamPage(int generation, const QString &episodeId, const QString &server,
                         const QString &audioMode, bool allowFallback);
  void applyEpisodes(const QString &animeId, int episodeCount);
  void setError(const QString &error);
  static QJsonObject payload(const QJsonObject &root);

  QNetworkAccessManager m_network;
  int m_pending = 0;
  QString m_error;
  QVariantList m_spotlight;
  QVariantList m_recent;
  QVariantList m_popular;
  QVariantList m_airing;
  QVariantList m_searchResults;
  QVariantMap m_details;
  QVariantList m_episodes;
  QVariantList m_recommendations;
  QVariantList m_subServers;
  QVariantList m_dubServers;
  bool m_searchHasMore = false;
};

class AccountClient final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticationChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(QVariantMap user READ user NOTIFY authenticationChanged)
  Q_PROPERTY(QVariantMap maintenance READ maintenance NOTIFY maintenanceChanged)
  Q_PROPERTY(QVariantList watchlist READ watchlist NOTIFY libraryChanged)
  Q_PROPERTY(QVariantList history READ history NOTIFY libraryChanged)
  Q_PROPERTY(QVariantList completed READ completed NOTIFY libraryChanged)
  Q_PROPERTY(QVariantList broadcasts READ broadcasts NOTIFY broadcastsChanged)
  Q_PROPERTY(QVariantMap playbackPreference READ playbackPreference NOTIFY preferenceChanged)

public:
  AccountClient(SecureStore *secureStore, Database *database, QObject *parent = nullptr);

  [[nodiscard]] bool busy() const { return m_pending > 0; }
  [[nodiscard]] bool authenticated() const { return !m_token.isEmpty(); }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] QVariantMap user() const { return m_user; }
  [[nodiscard]] QVariantMap maintenance() const { return m_maintenance; }
  [[nodiscard]] QVariantList watchlist() const { return m_watchlist; }
  [[nodiscard]] QVariantList history() const { return m_history; }
  [[nodiscard]] QVariantList completed() const { return m_completed; }
  [[nodiscard]] QVariantList broadcasts() const { return m_broadcasts; }
  [[nodiscard]] QVariantMap playbackPreference() const { return m_playbackPreference; }

  Q_INVOKABLE void restoreSession();
  Q_INVOKABLE void registerAccount(const QString &name, const QString &email, const QString &password);
  Q_INVOKABLE void verifyEmail(const QString &email, const QString &otp);
  Q_INVOKABLE void resendVerification(const QString &email);
  Q_INVOKABLE void login(const QString &email, const QString &password);
  Q_INVOKABLE void forgotPassword(const QString &email);
  Q_INVOKABLE void resetPassword(const QString &email, const QString &otp, const QString &newPassword);
  Q_INVOKABLE void logout();
  Q_INVOKABLE void refreshLibrary();
  Q_INVOKABLE void addToWatchlist(const QVariantMap &anime);
  Q_INVOKABLE void removeFromWatchlist(const QString &animeId);
  Q_INVOKABLE void deleteHistory(const QString &recordId);
  Q_INVOKABLE void saveProgress(const QVariantMap &progress);
  Q_INVOKABLE void loadResumeEpisode(const QString &episodeId);
  Q_INVOKABLE void loadResumeAnime(const QString &animeId);
  Q_INVOKABLE void loadPlaybackPreference();
  Q_INVOKABLE void setPlaybackPreference(const QVariantMap &preference);
  Q_INVOKABLE void refreshBroadcasts();
  Q_INVOKABLE void markBroadcastRead(const QString &id);
  Q_INVOKABLE void checkMaintenance();

signals:
  void busyChanged();
  void errorChanged();
  void authenticationChanged();
  void maintenanceChanged();
  void libraryChanged();
  void broadcastsChanged();
  void preferenceChanged();
  void resumeLoaded(const QString &kind, const QVariantMap &resume);
  void verificationRequired(const QString &email, int expiresInMinutes);
  void passwordResetRequested(const QString &email);
  void operationSucceeded(const QString &message);
  void sessionExpired();
  void localProgressSaved();

private:
  using Success = std::function<void(const QJsonObject &)>;
  void request(const QString &path, const QByteArray &method, const QJsonObject &body,
               bool authenticated, Success success,
               std::function<void(const QString &, int)> failure = {});
  void authenticateFrom(const QJsonObject &root);
  void setToken(const QString &token);
  void setError(const QString &error);
  void flushPendingProgress();
  static QVariantList objectList(const QJsonValue &value);
  static QJsonObject payload(const QJsonObject &root);

  SecureStore *m_secureStore = nullptr;
  Database *m_database = nullptr;
  QNetworkAccessManager m_network;
  QString m_token;
  QString m_error;
  int m_pending = 0;
  QVariantMap m_user;
  QVariantMap m_maintenance;
  QVariantList m_watchlist;
  QVariantList m_history;
  QVariantList m_completed;
  QVariantList m_broadcasts;
  QVariantMap m_playbackPreference;
};
