#include "SecureStore.h"

#include <QEventLoop>
#include <qtkeychain/keychain.h>
#include <QPointer>
#include <sodium.h>

namespace {
constexpr auto MasterKeyName = "native-master-key-v1";

template <typename Job>
bool waitForJob(Job *job, QString *error) {
  QEventLoop loop;
  QObject::connect(job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
  job->start();
  loop.exec();
  if (job->error() == QKeychain::NoError) {
    return true;
  }
  if (error) {
    *error = job->errorString();
  }
  return false;
}
}

SecureStore::SecureStore(QObject *parent, const QByteArray &initialMasterKey)
  : QObject(parent),
    m_service(QStringLiteral("ink.anicloud.desktop.native")),
    m_sodiumReady(sodium_init() >= 0),
    m_cachedMasterKey(initialMasterKey.size() == crypto_aead_xchacha20poly1305_ietf_KEYBYTES ? initialMasterKey : QByteArray{}) {}

QString SecureStore::readText(const QString &key, QString *error) const {
  QKeychain::ReadPasswordJob job(m_service);
  job.setKey(key);
  job.setAutoDelete(false);
  if (!waitForJob(&job, error)) {
    return {};
  }
  return job.textData();
}

bool SecureStore::writeText(const QString &key, const QString &value, QString *error) const {
  QKeychain::WritePasswordJob job(m_service);
  job.setKey(key);
  job.setTextData(value);
  job.setAutoDelete(false);
  job.setInsecureFallback(false);
  return waitForJob(&job, error);
}

bool SecureStore::remove(const QString &key, QString *error) const {
  QKeychain::DeletePasswordJob job(m_service);
  job.setKey(key);
  job.setAutoDelete(false);
  return waitForJob(&job, error);
}

QByteArray SecureStore::masterKey(QString *error) {
  if (!m_cachedMasterKey.isEmpty()) {
    return m_cachedMasterKey;
  }
  if (!m_sodiumReady) {
    if (error) *error = QStringLiteral("libsodium failed to initialize");
    return {};
  }

  const auto encoded = readText(QString::fromLatin1(MasterKeyName), error);
  if (!encoded.isEmpty()) {
    const auto decoded = QByteArray::fromBase64(encoded.toLatin1());
    if (decoded.size() == crypto_aead_xchacha20poly1305_ietf_KEYBYTES) {
      m_cachedMasterKey = decoded;
      return m_cachedMasterKey;
    }
  }

  QByteArray key(crypto_aead_xchacha20poly1305_ietf_KEYBYTES, Qt::Uninitialized);
  randombytes_buf(key.data(), static_cast<size_t>(key.size()));
  if (!writeText(QString::fromLatin1(MasterKeyName), QString::fromLatin1(key.toBase64()), error)) {
    sodium_memzero(key.data(), static_cast<size_t>(key.size()));
    return {};
  }
  m_cachedMasterKey = key;
  return m_cachedMasterKey;
}

QByteArray SecureStore::seal(const QByteArray &plainText, QString *error) {
  const auto key = masterKey(error);
  if (key.isEmpty()) return {};

  QByteArray nonce(crypto_aead_xchacha20poly1305_ietf_NPUBBYTES, Qt::Uninitialized);
  randombytes_buf(nonce.data(), static_cast<size_t>(nonce.size()));
  QByteArray cipher(plainText.size() + crypto_aead_xchacha20poly1305_ietf_ABYTES, Qt::Uninitialized);
  unsigned long long cipherLength = 0;
  if (crypto_aead_xchacha20poly1305_ietf_encrypt(
        reinterpret_cast<unsigned char *>(cipher.data()), &cipherLength,
        reinterpret_cast<const unsigned char *>(plainText.constData()), static_cast<unsigned long long>(plainText.size()),
        nullptr, 0, nullptr,
        reinterpret_cast<const unsigned char *>(nonce.constData()),
        reinterpret_cast<const unsigned char *>(key.constData())) != 0) {
    if (error) *error = QStringLiteral("Unable to encrypt local metadata");
    return {};
  }
  cipher.resize(static_cast<qsizetype>(cipherLength));
  return QByteArrayLiteral("ACN1") + nonce + cipher;
}

QByteArray SecureStore::open(const QByteArray &cipherText, QString *error) {
  const auto prefixSize = 4;
  const auto nonceSize = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
  if (!cipherText.startsWith(QByteArrayLiteral("ACN1")) || cipherText.size() <= prefixSize + nonceSize) {
    if (error) *error = QStringLiteral("Encrypted metadata has an unsupported format");
    return {};
  }
  const auto key = masterKey(error);
  if (key.isEmpty()) return {};
  const auto nonce = cipherText.mid(prefixSize, nonceSize);
  const auto cipher = cipherText.mid(prefixSize + nonceSize);
  QByteArray plain(cipher.size(), Qt::Uninitialized);
  unsigned long long plainLength = 0;
  if (crypto_aead_xchacha20poly1305_ietf_decrypt(
        reinterpret_cast<unsigned char *>(plain.data()), &plainLength, nullptr,
        reinterpret_cast<const unsigned char *>(cipher.constData()), static_cast<unsigned long long>(cipher.size()),
        nullptr, 0,
        reinterpret_cast<const unsigned char *>(nonce.constData()),
        reinterpret_cast<const unsigned char *>(key.constData())) != 0) {
    if (error) *error = QStringLiteral("Encrypted metadata failed authentication");
    return {};
  }
  plain.resize(static_cast<qsizetype>(plainLength));
  return plain;
}
