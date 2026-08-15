#include "HlsTools.h"

#include <QRegularExpression>
#include <limits>

namespace {
QString attribute(const QString &line, const QString &name) {
  const QRegularExpression expression(QStringLiteral("(?:^|,)%1=(?:\"([^\"]*)\"|([^,]*))")
                                      .arg(QRegularExpression::escape(name)),
                                      QRegularExpression::CaseInsensitiveOption);
  const auto match = expression.match(line.mid(line.indexOf(QLatin1Char(':')) + 1));
  return match.hasMatch() ? (match.captured(1).isEmpty() ? match.captured(2) : match.captured(1)).trimmed() : QString{};
}

QString replaceUriAttribute(const QString &line, const QUrl &baseUrl,
                            const std::function<QUrl(const QUrl &)> &mapUrl) {
  static const QRegularExpression expression(QStringLiteral("URI=(\"([^\"]*)\"|([^,]*))"),
                                              QRegularExpression::CaseInsensitiveOption);
  const auto match = expression.match(line);
  if (!match.hasMatch()) return line;
  const auto source = match.captured(2).isEmpty() ? match.captured(3) : match.captured(2);
  const auto mapped = mapUrl(baseUrl.resolved(QUrl(source))).toString(QUrl::FullyEncoded);
  auto output = line;
  output.replace(match.capturedStart(1), match.capturedLength(1), QStringLiteral("\"") + mapped + QStringLiteral("\""));
  return output;
}

QList<QString> lines(const QByteArray &manifest) {
  QList<QString> result;
  const auto decoded = QString::fromUtf8(manifest);
  for (const auto &line : decoded.split(QLatin1Char('\n'))) result.append(line.endsWith(QLatin1Char('\r')) ? line.chopped(1) : line);
  return result;
}
}

namespace HlsTools {

bool looksLikePlaylist(const QByteArray &body, const QUrl &url) {
  return body.trimmed().startsWith(QByteArrayLiteral("#EXTM3U")) ||
         url.path().endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive);
}

QList<Variant> variants(const QByteArray &manifest, const QUrl &baseUrl) {
  QList<Variant> result;
  const auto sourceLines = lines(manifest);
  for (qsizetype i = 0; i < sourceLines.size(); ++i) {
    const auto line = sourceLines.at(i).trimmed();
    if (!line.startsWith(QStringLiteral("#EXT-X-STREAM-INF:"), Qt::CaseInsensitive)) continue;
    qsizetype uriIndex = i + 1;
    while (uriIndex < sourceLines.size() && (sourceLines.at(uriIndex).trimmed().isEmpty() || sourceLines.at(uriIndex).trimmed().startsWith(QLatin1Char('#')))) ++uriIndex;
    if (uriIndex >= sourceLines.size()) continue;
    Variant item;
    item.url = baseUrl.resolved(QUrl(sourceLines.at(uriIndex).trimmed()));
    item.bandwidth = attribute(line, QStringLiteral("BANDWIDTH")).toInt();
    const auto resolution = attribute(line, QStringLiteral("RESOLUTION"));
    item.height = resolution.section(QLatin1Char('x'), 1, 1).toInt();
    item.codecs = attribute(line, QStringLiteral("CODECS"));
    if (item.url.isValid()) result.append(item);
  }
  return result;
}

Variant selectVariant(const QList<Variant> &items, int preferredHeight) {
  if (items.isEmpty()) return {};
  Variant best;
  int bestDistance = std::numeric_limits<int>::max();
  for (const auto &item : items) {
    const int height = item.height > 0 ? item.height : preferredHeight;
    const bool tooLarge = preferredHeight > 0 && height > preferredHeight;
    const int distance = qAbs(height - preferredHeight) + (tooLarge ? 100000 : 0);
    if (!best.url.isValid() || distance < bestDistance || (distance == bestDistance && item.bandwidth > best.bandwidth)) {
      best = item; bestDistance = distance;
    }
  }
  return best;
}

QList<Resource> resources(const QByteArray &manifest, const QUrl &baseUrl) {
  QList<Resource> result;
  QString pendingRange;
  for (const auto &rawLine : lines(manifest)) {
    const auto line = rawLine.trimmed();
    if (line.startsWith(QStringLiteral("#EXT-X-BYTERANGE:"), Qt::CaseInsensitive)) {
      pendingRange = line.section(QLatin1Char(':'), 1);
      continue;
    }
    if (line.startsWith(QStringLiteral("#EXT-X-KEY:"), Qt::CaseInsensitive) ||
        line.startsWith(QStringLiteral("#EXT-X-MAP:"), Qt::CaseInsensitive) ||
        line.startsWith(QStringLiteral("#EXT-X-MEDIA:"), Qt::CaseInsensitive)) {
      const auto uri = attribute(line, QStringLiteral("URI"));
      if (!uri.isEmpty()) result.append({baseUrl.resolved(QUrl(uri)), line.section(QLatin1Char(':'), 0, 0).mid(7).toLower(), pendingRange});
      pendingRange.clear();
      continue;
    }
    if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
      result.append({baseUrl.resolved(QUrl(line)), looksLikePlaylist({}, QUrl(line)) ? QStringLiteral("playlist") : QStringLiteral("segment"), pendingRange});
      pendingRange.clear();
    }
  }
  return result;
}

QByteArray rewrite(const QByteArray &manifest, const QUrl &baseUrl,
                   const std::function<QUrl(const QUrl &)> &mapUrl) {
  QStringList output;
  for (const auto &rawLine : lines(manifest)) {
    const auto line = rawLine.trimmed();
    if (line.startsWith(QStringLiteral("#EXT-X-KEY:"), Qt::CaseInsensitive) ||
        line.startsWith(QStringLiteral("#EXT-X-MAP:"), Qt::CaseInsensitive) ||
        line.startsWith(QStringLiteral("#EXT-X-MEDIA:"), Qt::CaseInsensitive) ||
        line.startsWith(QStringLiteral("#EXT-X-I-FRAME-STREAM-INF:"), Qt::CaseInsensitive)) {
      output.append(replaceUriAttribute(rawLine, baseUrl, mapUrl));
    } else if (!line.isEmpty() && !line.startsWith(QLatin1Char('#'))) {
      const auto leading = rawLine.left(rawLine.size() - rawLine.trimmed().size());
      output.append(leading + mapUrl(baseUrl.resolved(QUrl(line))).toString(QUrl::FullyEncoded));
    } else {
      output.append(rawLine);
    }
  }
  return output.join(QLatin1Char('\n')).toUtf8();
}

} // namespace HlsTools
