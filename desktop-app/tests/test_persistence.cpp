#include "Database.h"
#include "SecureStore.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>

class PersistenceTest final : public QObject {
  Q_OBJECT
private slots:
  void authenticatedEncryptionRejectsWrongKey() {
    SecureStore first(nullptr, QByteArray(32, 'a'));
    SecureStore second(nullptr, QByteArray(32, 'b'));
    QString error;
    const auto encrypted = first.seal(QByteArrayLiteral("sensitive-stream-metadata"), &error);
    QVERIFY2(!encrypted.isEmpty(), qPrintable(error));
    QCOMPARE(first.open(encrypted), QByteArrayLiteral("sensitive-stream-metadata"));
    QVERIFY(second.open(encrypted, &error).isEmpty());
    QVERIFY(!error.isEmpty());
  }

  void migratesAndPersistsLocalHistory() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    SecureStore store(nullptr, QByteArray(32, 'k'));
    Database database(&store, directory.path());
    QString error; QVERIFY2(database.open(&error), qPrintable(error));
    QVERIFY2(database.saveLocalProgress({
      {QStringLiteral("animeId"), QStringLiteral("anime-1")},
      {QStringLiteral("episodeId"), QStringLiteral("episode-1")},
      {QStringLiteral("animeName"), QStringLiteral("Example")},
      {QStringLiteral("episodeName"), QStringLiteral("Episode 1")},
      {QStringLiteral("positionSeconds"), 42},
      {QStringLiteral("durationSeconds"), 1200},
    }, &error), qPrintable(error));
    const auto history = database.localHistory();
    QCOMPARE(history.size(), 1);
    QCOMPARE(history.first().toMap().value(QStringLiteral("positionSeconds")).toInt(), 42);
    QVERIFY(QFileInfo::exists(directory.path() + QStringLiteral("/anicloud-native.db")));
  }

  void encryptsDownloadsAndIsolatesOwners() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    SecureStore store(nullptr, QByteArray(32, 'z'));
    Database database(&store, directory.path());
    QString error; QVERIFY2(database.open(&error), qPrintable(error));
    auto record = [](const QString &id, const QString &owner) {
      return QVariantMap{
        {QStringLiteral("id"), id}, {QStringLiteral("ownerId"), owner},
        {QStringLiteral("animeId"), QStringLiteral("anime")}, {QStringLiteral("animeName"), QStringLiteral("Anime")},
        {QStringLiteral("animeImage"), QStringLiteral("https://images.example/poster.jpg")},
        {QStringLiteral("episodeId"), QStringLiteral("episode")}, {QStringLiteral("episodeName"), QStringLiteral("Episode")},
        {QStringLiteral("episodeNumber"), 1}, {QStringLiteral("audioMode"), QStringLiteral("sub")},
        {QStringLiteral("qualityHeight"), 1080}, {QStringLiteral("server"), QStringLiteral("hd-1")},
        {QStringLiteral("state"), QStringLiteral("queued")}, {QStringLiteral("progress"), 0.0},
        {QStringLiteral("completedBytes"), 0}, {QStringLiteral("totalBytes"), 0},
        {QStringLiteral("rootPath"), QStringLiteral("/managed/") + id},
        {QStringLiteral("mediaUrl"), QStringLiteral("https://secret.example/master.m3u8")},
        {QStringLiteral("referer"), QStringLiteral("https://secret.example/")},
      };
    };
    QVERIFY(database.upsertDownload(record(QStringLiteral("one"), QStringLiteral("owner-a")), &error));
    QVERIFY(database.upsertDownload(record(QStringLiteral("two"), QStringLiteral("owner-b")), &error));
    QCOMPARE(database.downloads(QStringLiteral("owner-a")).size(), 1);
    QCOMPARE(database.downloads(QStringLiteral("owner-b")).size(), 1);
    QCOMPARE(database.downloads(QStringLiteral("owner-a")).first().toMap().value(QStringLiteral("mediaUrl")).toString(), QStringLiteral("https://secret.example/master.m3u8"));
    QVERIFY(database.upsertDownloadResource(QStringLiteral("one"), {
      {QStringLiteral("id"), QStringLiteral("segment")}, {QStringLiteral("url"), QStringLiteral("https://secret.example/segment.ts")},
      {QStringLiteral("relativePath"), QStringLiteral("resources/segment.ts")},
    }));
    QVERIFY(database.markDownloadResourceCompleted(QStringLiteral("one"), QStringLiteral("segment"), 1024));
    QCOMPARE(database.downloadResources(QStringLiteral("one")).first().toMap().value(QStringLiteral("size")).toInt(), 1024);
    for (const auto &name : {QStringLiteral("anicloud-native.db"), QStringLiteral("anicloud-native.db-wal")}) {
      QFile file(directory.path() + QLatin1Char('/') + name);
      if (file.open(QIODevice::ReadOnly)) QVERIFY(!file.readAll().contains("https://secret.example/master.m3u8"));
    }
  }

  void queuesProgressAndBroadcastReadState() {
    QTemporaryDir directory; QVERIFY(directory.isValid());
    SecureStore store(nullptr, QByteArray(32, 'q'));
    Database database(&store, directory.path());
    QVERIFY(database.open());
    QVERIFY(database.queueProgress(QStringLiteral("episode"), QStringLiteral("owner"), {{QStringLiteral("positionSeconds"), 90}}));
    const auto pending = database.pendingProgress(QStringLiteral("owner"));
    QCOMPARE(pending.size(), 1);
    QCOMPARE(pending.first().toMap().value(QStringLiteral("payload")).toMap().value(QStringLiteral("positionSeconds")).toInt(), 90);
    QVERIFY(database.replaceBroadcasts({QVariantMap{{QStringLiteral("id"), QStringLiteral("notice")}, {QStringLiteral("title"), QStringLiteral("Title")}, {QStringLiteral("message"), QStringLiteral("Message")}}}));
    QVERIFY(database.markBroadcastRead(QStringLiteral("notice")));
    QCOMPARE(database.broadcasts().first().toMap().value(QStringLiteral("read")).toInt(), 1);
  }
};

QTEST_GUILESS_MAIN(PersistenceTest)
#include "test_persistence.moc"
