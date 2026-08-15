#include "UpdateService.h"

#include <QJsonArray>
#include <QTest>
#include <sodium.h>

class UpdateServiceTest final : public QObject {
  Q_OBJECT
private slots:
  void comparesSemanticVersions() {
    QCOMPARE(UpdateService::compareVersions(QStringLiteral("4.0.2"), QStringLiteral("4.0.1")), 1);
    QCOMPARE(UpdateService::compareVersions(QStringLiteral("v4.0.1"), QStringLiteral("4.0.1")), 0);
    QCOMPARE(UpdateService::compareVersions(QStringLiteral("4.0.0"), QStringLiteral("4.0.1")), -1);
  }

  void verifiesDetachedEd25519Signature() {
    QVERIFY(sodium_init() >= 0);
    QByteArray publicKey(crypto_sign_PUBLICKEYBYTES, Qt::Uninitialized);
    QByteArray secretKey(crypto_sign_SECRETKEYBYTES, Qt::Uninitialized);
    crypto_sign_keypair(reinterpret_cast<unsigned char *>(publicKey.data()), reinterpret_cast<unsigned char *>(secretKey.data()));
    const QByteArray manifest("{\"version\":\"4.0.1\"}");
    QByteArray signature(crypto_sign_BYTES, Qt::Uninitialized); unsigned long long length = 0;
    crypto_sign_detached(reinterpret_cast<unsigned char *>(signature.data()), &length,
                         reinterpret_cast<const unsigned char *>(manifest.constData()), static_cast<unsigned long long>(manifest.size()),
                         reinterpret_cast<const unsigned char *>(secretKey.constData()));
    QString error;
    QVERIFY2(UpdateService::verifyManifest(manifest, signature, publicKey, &error), qPrintable(error));
    QVERIFY(!UpdateService::verifyManifest(manifest + ' ', signature, publicKey));
  }

  void selectsExactPlatformArchitecture() {
    QJsonObject manifest{{QStringLiteral("assets"), QJsonArray{
      QJsonObject{{QStringLiteral("platform"), QStringLiteral("linux")}, {QStringLiteral("architecture"), QStringLiteral("x64")}, {QStringLiteral("kind"), QStringLiteral("deb")}, {QStringLiteral("url"), QStringLiteral("deb-url")}},
      QJsonObject{{QStringLiteral("platform"), QStringLiteral("linux")}, {QStringLiteral("architecture"), QStringLiteral("x64")}, {QStringLiteral("kind"), QStringLiteral("appimage")}, {QStringLiteral("url"), QStringLiteral("app-url")}},
      QJsonObject{{QStringLiteral("platform"), QStringLiteral("macos")}, {QStringLiteral("architecture"), QStringLiteral("arm64")}, {QStringLiteral("kind"), QStringLiteral("dmg")}, {QStringLiteral("url"), QStringLiteral("mac-url")}},
    }}};
    QCOMPARE(UpdateService::selectAsset(manifest, QStringLiteral("linux"), QStringLiteral("x64")).value(QStringLiteral("url")).toString(), QStringLiteral("deb-url"));
    QVERIFY(UpdateService::selectAsset(manifest, QStringLiteral("windows"), QStringLiteral("x64")).isEmpty());
  }

  void prefersInstallersOverPortableAssets() {
    QJsonObject manifest{{QStringLiteral("assets"), QJsonArray{
      QJsonObject{{QStringLiteral("platform"), QStringLiteral("windows")}, {QStringLiteral("architecture"), QStringLiteral("x64")}, {QStringLiteral("kind"), QStringLiteral("portable")}, {QStringLiteral("url"), QStringLiteral("portable-url")}},
      QJsonObject{{QStringLiteral("platform"), QStringLiteral("windows")}, {QStringLiteral("architecture"), QStringLiteral("x64")}, {QStringLiteral("kind"), QStringLiteral("installer")}, {QStringLiteral("url"), QStringLiteral("installer-url")}},
    }}};
    QCOMPARE(UpdateService::selectAsset(manifest, QStringLiteral("windows"), QStringLiteral("x64")).value(QStringLiteral("url")).toString(), QStringLiteral("installer-url"));
  }
};

QTEST_GUILESS_MAIN(UpdateServiceTest)
#include "test_update.moc"
