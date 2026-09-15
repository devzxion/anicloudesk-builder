#include "ProviderCrypto.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRegularExpression>
#include <QUrl>
#include <QUrlQuery>
#include <openssl/evp.h>
#include <sodium.h>
#include <algorithm>

namespace {
const QRegularExpression EncodedValue(QStringLiteral("^[A-Za-z0-9_-]+={0,2}$"));
const QRegularExpression MediaIdentity(QStringLiteral("/([a-f0-9]{32})/([a-f0-9]{32})/"),
                                       QRegularExpression::CaseInsensitiveOption);

QByteArray base64UrlDecode(QByteArray value) {
  value.replace('-', '+');
  value.replace('_', '/');
  while (value.size() % 4) value.append('=');
  return QByteArray::fromBase64(value, QByteArray::AbortOnBase64DecodingErrors);
}

QByteArray base64UrlEncode(const QByteArray &value) {
  return value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
}

QString tokenFrom(const QString &url) {
  const QUrl parsed(url);
  const QUrlQuery query(parsed);
  return query.queryItemValue(QStringLiteral("token"), QUrl::FullyDecoded);
}
}

namespace ProviderCrypto {

QString decodeEncryptedFile(const QString &encoded, const QByteArray &key,
                            const QByteArray &iv) {
  if (encoded.size() < 24 || encoded.size() > 16'384 ||
      !EncodedValue.match(encoded).hasMatch() || iv.size() != 16 ||
      key.isEmpty() || key.size() > 32) return {};
  const auto ciphertext = base64UrlDecode(encoded.toLatin1());
  if (ciphertext.isEmpty() || ciphertext.size() % 16 != 0) return {};

  QByteArray paddedKey(32, '\0');
  std::copy_n(key.constData(), qMin(key.size(), paddedKey.size()), paddedKey.data());
  QByteArray plaintext(ciphertext.size() + EVP_MAX_BLOCK_LENGTH, Qt::Uninitialized);
  auto *context = EVP_CIPHER_CTX_new();
  if (!context) return {};
  int firstLength = 0;
  int finalLength = 0;
  const bool ok = EVP_DecryptInit_ex(context, EVP_aes_256_cbc(), nullptr,
                                     reinterpret_cast<const unsigned char *>(paddedKey.constData()),
                                     reinterpret_cast<const unsigned char *>(iv.constData())) == 1 &&
                  EVP_DecryptUpdate(context,
                                    reinterpret_cast<unsigned char *>(plaintext.data()), &firstLength,
                                    reinterpret_cast<const unsigned char *>(ciphertext.constData()),
                                    ciphertext.size()) == 1 &&
                  EVP_DecryptFinal_ex(context,
                                      reinterpret_cast<unsigned char *>(plaintext.data()) + firstLength,
                                      &finalLength) == 1;
  EVP_CIPHER_CTX_free(context);
  sodium_memzero(paddedKey.data(), static_cast<size_t>(paddedKey.size()));
  if (!ok) return {};
  plaintext.resize(firstLength + finalLength);
  const auto document = QJsonDocument::fromJson(plaintext);
  sodium_memzero(plaintext.data(), static_cast<size_t>(plaintext.size()));
  if (!document.isObject()) return {};
  const auto file = document.object().value(QStringLiteral("file")).toString().trimmed();
  const QUrl url(file);
  return url.isValid() && (url.scheme() == QStringLiteral("https") ||
                           url.scheme() == QStringLiteral("http")) ? file : QString{};
}

QString signMediaUrl(const QString &url, const QByteArray &tokenKey,
                     qint64 nowSeconds, int lifetimeSeconds) {
  const QUrl parsed(url);
  if (!parsed.isValid() || !parsed.fragment().isEmpty() ||
      (parsed.scheme() != QStringLiteral("https") && parsed.scheme() != QStringLiteral("http")) ||
      tokenKey.size() < 16 || tokenKey.size() > 128 ||
      lifetimeSeconds < 30 || lifetimeSeconds > 600) return {};
  const auto identityMatch = MediaIdentity.match(parsed.path());
  if (!identityMatch.hasMatch()) return {};
  const auto identity = identityMatch.captured(1).toLower() + QLatin1Char('/') +
                        identityMatch.captured(2).toLower();
  const auto body = QStringLiteral("%1|%2").arg(nowSeconds + lifetimeSeconds).arg(identity).toUtf8();
  const auto signature = QMessageAuthenticationCode::hash(
    body, tokenKey, QCryptographicHash::Sha256);
  auto output = parsed;
  QUrlQuery query(output);
  query.removeAllQueryItems(QStringLiteral("token"));
  query.addQueryItem(QStringLiteral("token"),
                     QString::fromLatin1(base64UrlEncode(body)) + QLatin1Char('.') +
                     QString::fromLatin1(base64UrlEncode(signature)));
  output.setQuery(query);
  return output.toString(QUrl::FullyEncoded);
}

QString refreshSignedUrl(const QString &url, const QByteArray &tokenKey,
                         qint64 nowSeconds, int lifetimeSeconds,
                         int refreshLeadSeconds) {
  if (tokenKey.isEmpty()) return url;
  const auto token = tokenFrom(url);
  const auto separator = token.indexOf(QLatin1Char('.'));
  if (separator <= 0) return url;
  const auto body = base64UrlDecode(token.left(separator).toLatin1());
  const auto suppliedSignature = base64UrlDecode(token.mid(separator + 1).toLatin1());
  bool expiresOk = false;
  const auto expiresAt = QString::fromUtf8(body).section(QLatin1Char('|'), 0, 0).toLongLong(&expiresOk);
  if (!expiresOk || suppliedSignature.size() != 32) return url;
  const auto expectedSignature = QMessageAuthenticationCode::hash(
    body, tokenKey, QCryptographicHash::Sha256);
  if (sodium_memcmp(expectedSignature.constData(), suppliedSignature.constData(),
                    static_cast<size_t>(expectedSignature.size())) != 0) return url;
  if (refreshLeadSeconds < 5 || refreshLeadSeconds >= lifetimeSeconds) return url;
  if (expiresAt > nowSeconds + refreshLeadSeconds) return url;
  const auto refreshed = signMediaUrl(url, tokenKey, nowSeconds, lifetimeSeconds);
  return refreshed.isEmpty() ? url : refreshed;
}

QJsonObject decodeXorPayload(const QString &encoded, const QByteArray &key) {
  if (encoded.isEmpty() || encoded.size() > 65'536 || key.isEmpty() || key.size() > 128) return {};
  auto bytes = QByteArray::fromBase64(encoded.toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
  if (bytes.isEmpty()) bytes = base64UrlDecode(encoded.toLatin1());
  if (bytes.isEmpty()) return {};
  for (qsizetype index = 0; index < bytes.size(); ++index)
    bytes[index] = static_cast<char>(bytes.at(index) ^ key.at(index % key.size()));
  const auto document = QJsonDocument::fromJson(bytes);
  sodium_memzero(bytes.data(), static_cast<size_t>(bytes.size()));
  return document.isObject() ? document.object() : QJsonObject{};
}

} // namespace ProviderCrypto
