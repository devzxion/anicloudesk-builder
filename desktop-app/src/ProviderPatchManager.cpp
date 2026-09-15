#include "ProviderPatchManager.h"

#include "BuildConfig.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrlQuery>
#include <openssl/evp.h>
#include <openssl/x509.h>

namespace {
constexpr qsizetype MaxEnvelopeBytes = 65'536;
constexpr qsizetype MaxPayloadBytes = 32'768;
constexpr qint64 RefreshIntervalMs = 15 * 60 * 1000;

bool exactKeys(const QJsonObject &object, const QSet<QString> &allowed) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!allowed.contains(it.key())) return false;
  return true;
}

bool verifyRsaSha256(const QByteArray &payload, const QByteArray &signature,
                     const QByteArray &publicKeyDer) {
  const auto *cursor = reinterpret_cast<const unsigned char *>(publicKeyDer.constData());
  EVP_PKEY *key = d2i_PUBKEY(nullptr, &cursor, publicKeyDer.size());
  EVP_MD_CTX *context = EVP_MD_CTX_new();
  const bool ok = key && context &&
    EVP_DigestVerifyInit(context, nullptr, EVP_sha256(), nullptr, key) == 1 &&
    EVP_DigestVerifyUpdate(context, payload.constData(), static_cast<size_t>(payload.size())) == 1 &&
    EVP_DigestVerifyFinal(context,
      reinterpret_cast<const unsigned char *>(signature.constData()),
      static_cast<size_t>(signature.size())) == 1;
  EVP_MD_CTX_free(context);
  EVP_PKEY_free(key);
  return ok;
}
}

ProviderPatchManager::ProviderPatchManager(QObject *parent) : QObject(parent) {
  const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  QDir().mkpath(directory);
  m_path = QDir(directory).filePath(QStringLiteral("provider-rules-v1.json"));
  load();
}

ProviderRules ProviderPatchManager::activeRules() const {
  return m_installed.revision > 0 && m_installed.expiresAt > QDateTime::currentSecsSinceEpoch()
    ? m_installed.rules : ProviderRules{};
}

bool ProviderPatchManager::verifyEnvelope(const QByteArray &envelope,
                                          const QByteArray &publicKeyBase64,
                                          int versionCode, qint64 nowSeconds,
                                          ProviderPatch *patch, bool allowExpired) {
  if (!patch || envelope.isEmpty() || envelope.size() > MaxEnvelopeBytes) return false;
  const auto signedDocument = QJsonDocument::fromJson(envelope);
  if (!signedDocument.isObject()) return false;
  const auto signedObject = signedDocument.object();
  if (!exactKeys(signedObject, {QStringLiteral("payload"), QStringLiteral("signature")})) return false;
  const auto payload = QByteArray::fromBase64(signedObject.value(QStringLiteral("payload")).toString().toLatin1(),
                                               QByteArray::AbortOnBase64DecodingErrors);
  const auto signature = QByteArray::fromBase64(signedObject.value(QStringLiteral("signature")).toString().toLatin1(),
                                                 QByteArray::AbortOnBase64DecodingErrors);
  const auto publicKey = QByteArray::fromBase64(publicKeyBase64.trimmed(),
                                                 QByteArray::AbortOnBase64DecodingErrors);
  if (payload.isEmpty() || payload.size() > MaxPayloadBytes || signature.isEmpty() ||
      publicKey.isEmpty() || !verifyRsaSha256(payload, signature, publicKey)) return false;
  const auto document = QJsonDocument::fromJson(payload);
  if (!document.isObject()) return false;
  const auto root = document.object();
  const QSet<QString> allowed{QStringLiteral("schemaVersion"), QStringLiteral("revision"),
                              QStringLiteral("minVersionCode"), QStringLiteral("maxVersionCode"),
                              QStringLiteral("issuedAt"), QStringLiteral("expiresAt"),
                              QStringLiteral("rules")};
  if (!exactKeys(root, allowed) || root.value(QStringLiteral("schemaVersion")).toInt() != 1 ||
      !root.value(QStringLiteral("rules")).isObject()) return false;
  const auto revision = root.value(QStringLiteral("revision")).toVariant().toLongLong();
  const auto minimum = root.value(QStringLiteral("minVersionCode")).toInt();
  const auto maximum = root.value(QStringLiteral("maxVersionCode")).toInt();
  const auto issuedAt = root.value(QStringLiteral("issuedAt")).toVariant().toLongLong();
  const auto expiresAt = root.value(QStringLiteral("expiresAt")).toVariant().toLongLong();
  if (revision <= 0 || versionCode < minimum || versionCode > maximum || issuedAt <= 0 ||
      issuedAt > nowSeconds + 300 || expiresAt <= issuedAt ||
      expiresAt - issuedAt > 366LL * 86'400 || (!allowExpired && expiresAt <= nowSeconds)) return false;
  bool rulesOk = false;
  auto rules = ProviderRules::fromJson(root.value(QStringLiteral("rules")).toObject(), &rulesOk);
  if (!rulesOk) return false;
  patch->revision = revision;
  patch->expiresAt = expiresAt;
  patch->rules = std::move(rules);
  return true;
}

void ProviderPatchManager::load() {
  QFile file(m_path);
  if (!file.open(QIODevice::ReadOnly) || file.size() > MaxEnvelopeBytes) return;
  install(file.readAll(), false);
}

bool ProviderPatchManager::install(const QByteArray &envelope, bool persist) {
  ProviderPatch candidate;
  if (!verifyEnvelope(envelope, QByteArrayLiteral(ANICLOUD_PROVIDER_PATCH_PUBLIC_KEY_BASE64),
                      ANICLOUD_VERSION_CODE, QDateTime::currentSecsSinceEpoch(), &candidate,
                      !persist) || candidate.revision <= m_installed.revision) return false;
  if (persist) {
    QSaveFile output(m_path);
    if (!output.open(QIODevice::WriteOnly) || output.write(envelope) != envelope.size() ||
        !output.commit()) return false;
  }
  const auto before = activeRules();
  m_installed = std::move(candidate);
  if (before.primaryBaseUrl != activeRules().primaryBaseUrl ||
      before.secondaryBaseUrl != activeRules().secondaryBaseUrl ||
      before.primaryCipherKey != activeRules().primaryCipherKey ||
      before.primaryTokenKey != activeRules().primaryTokenKey) emit rulesChanged();
  return true;
}

void ProviderPatchManager::refresh() {
  const auto nowMs = QDateTime::currentMSecsSinceEpoch();
  if (m_lastAttemptMs > 0 && nowMs - m_lastAttemptMs < RefreshIntervalMs) return;
  m_lastAttemptMs = nowMs;
  QUrl url(QString::fromUtf8(ANICLOUD_ACCOUNT_API_BASE_URL) +
           QStringLiteral("/providers/patch/latest"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("versionCode"), QString::number(ANICLOUD_VERSION_CODE));
  query.addQueryItem(QStringLiteral("platform"), QStringLiteral("desktop"));
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(20'000);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/json"));
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto body = reply->read(MaxEnvelopeBytes + 1);
    if (reply->error() == QNetworkReply::NoError && status == 200 &&
        body.size() <= MaxEnvelopeBytes) install(body, true);
    reply->deleteLater();
  });
}
