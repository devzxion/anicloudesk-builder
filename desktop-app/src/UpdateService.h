#pragma once

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QUrl>
#include <QVariantMap>

class QFile;
class QNetworkReply;

class UpdateService final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
  Q_PROPERTY(bool updateAvailable READ updateAvailable NOTIFY updateChanged)
  Q_PROPERTY(bool mandatory READ mandatory NOTIFY updateChanged)
  Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
  Q_PROPERTY(QString availableVersion READ availableVersion NOTIFY updateChanged)

public:
  explicit UpdateService(QObject *parent = nullptr);

  [[nodiscard]] QString status() const { return m_status; }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] bool checking() const { return m_checking; }
  [[nodiscard]] bool updateAvailable() const { return !m_asset.isEmpty(); }
  [[nodiscard]] bool mandatory() const { return updateAvailable(); }
  [[nodiscard]] double progress() const { return m_progress; }
  [[nodiscard]] QString availableVersion() const { return m_version; }

  Q_INVOKABLE void check();
  Q_INVOKABLE void downloadAndInstall();
  Q_INVOKABLE bool launchInstaller();

  [[nodiscard]] static int compareVersions(const QString &left, const QString &right);
  [[nodiscard]] static bool verifyManifest(const QByteArray &manifest, const QByteArray &signature,
                                           const QByteArray &publicKey, QString *error = nullptr);
  [[nodiscard]] static QVariantMap selectAsset(const QJsonObject &manifest, const QString &platform,
                                               const QString &architecture);

signals:
  void statusChanged();
  void errorChanged();
  void checkingChanged();
  void updateChanged();
  void progressChanged();

private:
  void fetchSignature(const QByteArray &manifest);
  void acceptManifest(const QByteArray &manifest, const QByteArray &signature);
  void setStatus(const QString &status);
  void setError(const QString &error);
  static QString platformName();
  static QString architectureName();
  static QByteArray decodeSignature(const QByteArray &signature);

  QNetworkAccessManager m_network;
  QString m_status = QStringLiteral("idle");
  QString m_error;
  QString m_version;
  QVariantMap m_asset;
  QString m_installerPath;
  double m_progress = 0.0;
  bool m_checking = false;
};
