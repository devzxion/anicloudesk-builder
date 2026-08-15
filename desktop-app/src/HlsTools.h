#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>
#include <functional>

namespace HlsTools {

struct Variant {
  QUrl url;
  int bandwidth = 0;
  int height = 0;
  QString codecs;
};

struct Resource {
  QUrl url;
  QString kind;
  QString byteRange;
};

[[nodiscard]] bool looksLikePlaylist(const QByteArray &body, const QUrl &url = {});
[[nodiscard]] QList<Variant> variants(const QByteArray &manifest, const QUrl &baseUrl);
[[nodiscard]] Variant selectVariant(const QList<Variant> &items, int preferredHeight);
[[nodiscard]] QList<Resource> resources(const QByteArray &manifest, const QUrl &baseUrl);
[[nodiscard]] QByteArray rewrite(const QByteArray &manifest, const QUrl &baseUrl,
                                 const std::function<QUrl(const QUrl &)> &mapUrl);

} // namespace HlsTools
