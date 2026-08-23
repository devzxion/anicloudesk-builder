#include "SubtitleTools.h"

#include <QRegularExpression>
#include <QtMath>

namespace {
qint64 timestampMilliseconds(QString value) {
  value = value.trimmed();
  value.replace(QLatin1Char(','), QLatin1Char('.'));
  const auto fields = value.split(QLatin1Char(':'));
  if (fields.size() < 2 || fields.size() > 3) return -1;
  bool ok = false;
  const auto seconds = fields.last().toDouble(&ok);
  if (!ok) return -1;
  const auto minutes = fields.at(fields.size() - 2).toLongLong(&ok);
  if (!ok) return -1;
  qint64 hours = 0;
  if (fields.size() == 3) {
    hours = fields.first().toLongLong(&ok);
    if (!ok) return -1;
  }
  return hours * 3'600'000 + minutes * 60'000 + qRound64(seconds * 1000.0);
}

QString cleanText(QString value) {
  value.remove(QRegularExpression(QStringLiteral("<[^>]+>")));
  value.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
  value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
  value.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
  value.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
  value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
  value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
  return value.trimmed();
}
}

namespace SubtitleTools {

QList<Cue> parse(const QByteArray &document) {
  auto source = QString::fromUtf8(document);
  source.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
  source.replace(QLatin1Char('\r'), QLatin1Char('\n'));
  const auto lines = source.split(QLatin1Char('\n'));
  static const QRegularExpression timing(QStringLiteral(
    "^\\s*((?:\\d{1,2}:)?\\d{1,2}:\\d{2}[.,]\\d{3})\\s*-->\\s*"
    "((?:\\d{1,2}:)?\\d{1,2}:\\d{2}[.,]\\d{3})(?:\\s+.*)?$"));

  QList<Cue> result;
  qint64 timestampOffset = 0;
  for (qsizetype index = 0; index < lines.size(); ++index) {
    if (lines.at(index).contains(QStringLiteral("X-TIMESTAMP-MAP"), Qt::CaseInsensitive)) {
      const auto local = QRegularExpression(QStringLiteral("LOCAL:([^,\\s]+)"), QRegularExpression::CaseInsensitiveOption).match(lines.at(index));
      const auto mpegts = QRegularExpression(QStringLiteral("MPEGTS:(\\d+)"), QRegularExpression::CaseInsensitiveOption).match(lines.at(index));
      if (local.hasMatch() && mpegts.hasMatch()) {
        const auto localTime = timestampMilliseconds(local.captured(1));
        const auto mediaTime = qRound64(mpegts.captured(1).toDouble() * 1000.0 / 90'000.0);
        if (localTime >= 0) timestampOffset = mediaTime - localTime;
      }
      continue;
    }
    const auto match = timing.match(lines.at(index));
    if (!match.hasMatch()) continue;
    Cue cue;
    cue.start = timestampMilliseconds(match.captured(1)) + timestampOffset;
    cue.end = timestampMilliseconds(match.captured(2)) + timestampOffset;
    QStringList text;
    while (++index < lines.size() && !lines.at(index).trimmed().isEmpty())
      text.append(lines.at(index).trimmed());
    cue.text = cleanText(text.join(QLatin1Char('\n')));
    if (cue.start >= 0 && cue.end > cue.start && !cue.text.isEmpty()) result.append(cue);
  }
  return result;
}

QString textAt(const QList<Cue> &cues, qint64 positionMilliseconds) {
  QStringList visible;
  for (const auto &cue : cues) {
    if (cue.start > positionMilliseconds) break;
    if (positionMilliseconds >= cue.start && positionMilliseconds < cue.end) visible.append(cue.text);
  }
  visible.removeDuplicates();
  return visible.join(QLatin1Char('\n'));
}

} // namespace SubtitleTools
