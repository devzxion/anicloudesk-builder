#include "ProviderCrypto.h"
#include "ProviderPatchManager.h"
#include "ProviderRules.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QTest>
#include <QUrl>
#include <QUrlQuery>

class ProviderCryptoTest final : public QObject {
  Q_OBJECT
private:
  static QByteArray fixture(const QString &name) {
    QFile file(QStringLiteral(ANICLOUD_FIXTURE_DIR "/") + name);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
  }

private slots:
  void decodesCapturedEncryptedSource() {
    const auto root = QJsonDocument::fromJson(fixture(QStringLiteral("provider_encrypted.json"))).object();
    const auto file = ProviderCrypto::decodeEncryptedFile(
      root.value(QStringLiteral("enc")).toString(), QByteArrayLiteral("i?LMTAx0Q6,:}50U"),
      QByteArrayLiteral("W0;27ToaUpl_P%'c"));
    QCOMPARE(file, QStringLiteral("https://media.example/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/master.m3u8"));
  }

  void rejectsInvalidCiphertext() {
    QVERIFY(ProviderCrypto::decodeEncryptedFile(QStringLiteral("not-ciphertext"),
      QByteArrayLiteral("i?LMTAx0Q6,:}50U"), QByteArrayLiteral("W0;27ToaUpl_P%'c")).isEmpty());
  }

  void signsAndRenewsOnlyAuthenticTokens() {
    const auto key = QByteArrayLiteral("MpCdnT0k3n!9f2K#xQ7vL5mR8wN1pY4s");
    const auto source = QStringLiteral("https://media.example/aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb/master.m3u8?quality=auto");
    const auto signedUrl = ProviderCrypto::signMediaUrl(source, key, 1'000);
    QVERIFY(!QUrlQuery(QUrl(signedUrl)).queryItemValue(QStringLiteral("token")).isEmpty());
    QCOMPARE(ProviderCrypto::refreshSignedUrl(signedUrl, key, 1'020), signedUrl);
    const auto renewed = ProviderCrypto::refreshSignedUrl(signedUrl, key, 1'080);
    QVERIFY(renewed != signedUrl);
    QVERIFY(renewed.contains(QStringLiteral("quality=auto")));
    auto tampered = signedUrl;
    tampered.replace(tampered.size() - 1, 1, QStringLiteral("x"));
    QCOMPARE(ProviderCrypto::refreshSignedUrl(tampered, key, 1'080), tampered);
  }

  void decodesZokoPayloadAndMapsSkipData() {
    const auto page = QString::fromUtf8(fixture(QStringLiteral("zoko_payload.txt")));
    const QRegularExpression expression(QStringLiteral("window\\.__P=\\\"([^\\\"]+)\\\""));
    const auto payload = ProviderCrypto::decodeXorPayload(
      expression.match(page).captured(1), QByteArrayLiteral("otaku-embed-v1"));
    QCOMPARE(payload.value(QStringLiteral("sources")).toArray().first().toObject()
             .value(QStringLiteral("file")).toString(),
             QStringLiteral("https://zoko.example/video/master.m3u8"));
    QCOMPARE(payload.value(QStringLiteral("skip")).toObject().value(QStringLiteral("intro"))
             .toObject().value(QStringLiteral("end")).toInt(), 82);
  }

  void preservesAndroidFallbackAndCatalogOrder() {
    QCOMPARE(providerFallbackOrder(QStringLiteral("hd-1")),
             QStringList({QStringLiteral("hd-1"), QStringLiteral("hd-3"), QStringLiteral("hd-4")}));
    QCOMPARE(providerFallbackOrder(QStringLiteral("hd-2")),
             QStringList({QStringLiteral("hd-2"), QStringLiteral("hd-4"), QStringLiteral("hd-3")}));
    const auto hd3 = zokoRouteCandidates(QStringLiteral("hd-3"), QStringLiteral("21"), QStringLiteral("30013"));
    QCOMPARE(hd3.first(), qMakePair(QStringLiteral("ani"), QStringLiteral("30013")));
    QCOMPARE(hd3.last(), qMakePair(QStringLiteral("mal"), QStringLiteral("21")));
    const auto hd4 = zokoRouteCandidates(QStringLiteral("hd-4"), QStringLiteral("21"), QStringLiteral("30013"));
    QCOMPARE(hd4.first(), qMakePair(QStringLiteral("mal"), QStringLiteral("21")));
    QCOMPARE(hd4.last(), qMakePair(QStringLiteral("ani"), QStringLiteral("30013")));
  }

  void rejectsUnsignedProviderPatch() {
    ProviderPatch patch;
    const QByteArray envelope = R"({"payload":"e30=","signature":"AAAA"})";
    QVERIFY(!ProviderPatchManager::verifyEnvelope(envelope, QByteArrayLiteral("AAAA"),
                                                   40105, 1'000, &patch));
  }

  void rejectsUnsafeOrUnknownProviderRules() {
    bool ok = true;
    ProviderRules::fromJson(QJsonObject{
      {QStringLiteral("primaryBaseUrl"), QStringLiteral("http://127.0.0.1/provider")},
    }, &ok);
    QVERIFY(!ok);
    ProviderRules::fromJson(QJsonObject{
      {QStringLiteral("unexpectedCredential"), QStringLiteral("value")},
    }, &ok);
    QVERIFY(!ok);
  }
};

QTEST_GUILESS_MAIN(ProviderCryptoTest)
#include "test_providercrypto.moc"
