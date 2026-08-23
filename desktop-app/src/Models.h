#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

namespace AniCloud {

struct AnimeCard {
  QString id;
  QString title;
  QString alternativeTitle;
  QString poster;
  QString type;
  QString duration;
  QString synopsis;
  int subEpisodes = 0;
  int dubEpisodes = 0;
  int episodes = 0;

  [[nodiscard]] QVariantMap toVariant() const {
    return {
      {QStringLiteral("id"), id},
      {QStringLiteral("title"), title},
      {QStringLiteral("alternativeTitle"), alternativeTitle},
      {QStringLiteral("poster"), poster},
      {QStringLiteral("type"), type},
      {QStringLiteral("duration"), duration},
      {QStringLiteral("synopsis"), synopsis},
      {QStringLiteral("subEpisodes"), subEpisodes},
      {QStringLiteral("dubEpisodes"), dubEpisodes},
      {QStringLiteral("episodes"), episodes},
    };
  }
};

struct Episode {
  QString id;
  QString animeId;
  int number = 0;
  QString title;
  QString alternativeTitle;
  bool filler = false;

  [[nodiscard]] QVariantMap toVariant() const {
    return {
      {QStringLiteral("id"), id},
      {QStringLiteral("animeId"), animeId},
      {QStringLiteral("number"), number},
      {QStringLiteral("title"), title},
      {QStringLiteral("alternativeTitle"), alternativeTitle},
      {QStringLiteral("filler"), filler},
    };
  }
};

struct SubtitleTrack {
  QString url;
  QString label;
  QString language;
  bool isDefault = false;

  [[nodiscard]] QVariantMap toVariant() const {
    return {
      {QStringLiteral("url"), url},
      {QStringLiteral("label"), label},
      {QStringLiteral("language"), language},
      {QStringLiteral("default"), isDefault},
    };
  }
};

struct StreamDescriptor {
  QString episodeId;
  QString mediaUrl;
  QString server = QStringLiteral("hd-2");
  QString audioMode = QStringLiteral("sub");
  QString referer;
  QVariantMap headers;
  QVariantList subtitles;
  QVariantList alternates;
  qint64 introStart = 0;
  qint64 introEnd = 0;
  qint64 outroStart = 0;
  qint64 outroEnd = 0;

  [[nodiscard]] QVariantMap toVariant() const {
    return {
      {QStringLiteral("episodeId"), episodeId},
      {QStringLiteral("mediaUrl"), mediaUrl},
      {QStringLiteral("server"), server},
      {QStringLiteral("audioMode"), audioMode},
      {QStringLiteral("referer"), referer},
      {QStringLiteral("headers"), headers},
      {QStringLiteral("subtitles"), subtitles},
      {QStringLiteral("alternates"), alternates},
      {QStringLiteral("introStart"), introStart},
      {QStringLiteral("introEnd"), introEnd},
      {QStringLiteral("outroStart"), outroStart},
      {QStringLiteral("outroEnd"), outroEnd},
    };
  }
};

inline QVariantList variants(const QList<AnimeCard> &items) {
  QVariantList result;
  result.reserve(items.size());
  for (const auto &item : items) {
    result.append(item.toVariant());
  }
  return result;
}

inline QVariantList variants(const QList<Episode> &items) {
  QVariantList result;
  result.reserve(items.size());
  for (const auto &item : items) {
    result.append(item.toVariant());
  }
  return result;
}

} // namespace AniCloud

Q_DECLARE_METATYPE(AniCloud::StreamDescriptor)
