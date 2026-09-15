#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QPair>

struct ProviderRules {
  QString primaryBaseUrl = QStringLiteral("https://megaplay.buzz");
  QString secondaryBaseUrl = QStringLiteral("https://zokoanime.video");
  QString primaryPagePath = QStringLiteral("stream/mal/{animeId}/{episode}/{audio}");
  QString secondaryPagePath = QStringLiteral("stream/{catalog}/{animeId}/{episode}/{audio}");
  QStringList primarySourcesPaths{QStringLiteral("stream/getSourcesNew"),
                                  QStringLiteral("stream/getSources")};
  QString secondarySourcesPath = QStringLiteral("stream/getSources");
  QString sourceIdParameter = QStringLiteral("id");
  QString sourcesField = QStringLiteral("sources");
  QString sourceFileField = QStringLiteral("file");
  QString tracksField = QStringLiteral("tracks");
  QByteArray primaryCipherKey = QByteArrayLiteral("i?LMTAx0Q6,:}50U");
  QByteArray primaryCipherIv = QByteArrayLiteral("W0;27ToaUpl_P%'c");
  QByteArray primaryTokenKey = QByteArrayLiteral("MpCdnT0k3n!9f2K#xQ7vL5mR8wN1pY4s");
  int primaryTokenLifetimeSeconds = 90;
  int primaryTokenRefreshLeadSeconds = 30;
  QByteArray secondaryPayloadKey = QByteArrayLiteral("otaku-embed-v1");
  QMap<QString, QString> primaryMediaHeaders;
  QMap<QString, QString> secondaryMediaHeaders;

  [[nodiscard]] QString pagePath(const QString &pattern, const QString &animeId,
                                 int episode, const QString &audio,
                                 const QString &catalog = QStringLiteral("mal")) const;
  [[nodiscard]] bool valid(QString *error = nullptr) const;
  static ProviderRules fromJson(const QJsonObject &object, bool *ok = nullptr,
                                QString *error = nullptr);
};

struct ProviderPatch {
  qint64 revision = 0;
  qint64 expiresAt = 0;
  ProviderRules rules;
};

QStringList providerFallbackOrder(const QString &requested);
QList<QPair<QString, QString>> zokoRouteCandidates(const QString &server,
                                                   const QString &malId,
                                                   const QString &anilistId);
