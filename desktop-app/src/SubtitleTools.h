#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

namespace SubtitleTools {

struct Cue {
  qint64 start = 0;
  qint64 end = 0;
  QString text;
};

[[nodiscard]] QList<Cue> parse(const QByteArray &document);
[[nodiscard]] QString textAt(const QList<Cue> &cues, qint64 positionMilliseconds);

} // namespace SubtitleTools
