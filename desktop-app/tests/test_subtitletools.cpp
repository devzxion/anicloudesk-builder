#include "SubtitleTools.h"

#include <QTest>

class SubtitleToolsTest final : public QObject {
  Q_OBJECT
private slots:
  void parsesWebVttAndSrtTimestamps() {
    const QByteArray source(
      "WEBVTT\n\n00:00:01.000 --> 00:00:03.500 align:middle\n<b>Hello</b> &amp; welcome\n\n"
      "2\n00:04:05,250 --> 00:04:07,000\nSecond line\n");
    const auto cues = SubtitleTools::parse(source);
    QCOMPARE(cues.size(), 2);
    QCOMPARE(cues.first().start, 1000);
    QCOMPARE(cues.first().text, QStringLiteral("Hello & welcome"));
    QCOMPARE(cues.last().start, 245250);
  }

  void returnsOnlyCurrentlyVisibleCues() {
    const QList<SubtitleTools::Cue> cues{
      {1000, 3000, QStringLiteral("First")},
      {2000, 4000, QStringLiteral("Second")},
    };
    QVERIFY(SubtitleTools::textAt(cues, 500).isEmpty());
    QCOMPARE(SubtitleTools::textAt(cues, 2500), QStringLiteral("First\nSecond"));
    QCOMPARE(SubtitleTools::textAt(cues, 3500), QStringLiteral("Second"));
  }

  void appliesHlsTimestampMaps() {
    const QByteArray source(
      "WEBVTT\nX-TIMESTAMP-MAP=LOCAL:00:00:00.000,MPEGTS:900000\n\n"
      "00:00:01.000 --> 00:00:02.000\nMapped cue\n");
    const auto cues = SubtitleTools::parse(source);
    QCOMPARE(cues.size(), 1);
    QCOMPARE(cues.first().start, 11000);
    QCOMPARE(SubtitleTools::textAt(cues, 11500), QStringLiteral("Mapped cue"));
  }
};

QTEST_GUILESS_MAIN(SubtitleToolsTest)
#include "test_subtitletools.moc"
