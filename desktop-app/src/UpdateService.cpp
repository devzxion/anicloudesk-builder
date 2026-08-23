#include "UpdateService.h"

#include "BuildConfig.h"

#include <QCryptographicHash>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QMessageBox>
#include <QStandardPaths>
#include <QSysInfo>
#include <QThread>
#include <sodium.h>

#if defined(Q_OS_WIN)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#else
#include <cerrno>
#include <csignal>
#endif

namespace {
QUrl releaseUrl(const QString &name) {
  return QUrl(QStringLiteral("https://github.com/%1/releases/latest/download/%2")
                .arg(QString::fromUtf8(ANICLOUD_RELEASE_REPOSITORY), name));
}

QList<int> versionParts(const QString &version) {
  QList<int> result;
  const auto clean = version.trimmed().remove(QRegularExpression(QStringLiteral("^[^0-9]*"))).section(QLatin1Char('-'), 0, 0);
  for (const auto &part : clean.split(QLatin1Char('.'))) result.append(part.toInt());
  while (result.size() < 3) result.append(0);
  return result;
}

bool processIsRunning(qint64 processId) {
  if (processId <= 0) return false;
#if defined(Q_OS_WIN)
  const HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(processId));
  if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
  const bool running = WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
  CloseHandle(process);
  return running;
#else
  return ::kill(static_cast<pid_t>(processId), 0) == 0 || errno == EPERM;
#endif
}

bool launchInstallerDirect(const QString &installerPath) {
#if defined(Q_OS_WIN)
  const auto file = QDir::toNativeSeparators(installerPath).toStdWString();
  const auto directory = QDir::toNativeSeparators(QFileInfo(installerPath).absolutePath()).toStdWString();
  SHELLEXECUTEINFOW execute{};
  execute.cbSize = sizeof(execute);
  execute.fMask = SEE_MASK_NOCLOSEPROCESS;
  execute.lpVerb = L"runas";
  execute.lpFile = file.c_str();
  execute.lpDirectory = directory.c_str();
  execute.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&execute)) return false;
  if (execute.hProcess) CloseHandle(execute.hProcess);
  return true;
#elif defined(Q_OS_MACOS)
  return QProcess::startDetached(QStringLiteral("open"), {installerPath});
#else
  if (installerPath.endsWith(QStringLiteral(".AppImage"), Qt::CaseInsensitive)) {
    QFile::setPermissions(installerPath, QFile::permissions(installerPath) | QFileDevice::ExeUser);
    return QProcess::startDetached(installerPath, {});
  }
  return QProcess::startDetached(QStringLiteral("xdg-open"), {installerPath});
#endif
}
}

UpdateService::UpdateService(QObject *parent) : QObject(parent) {}

void UpdateService::setStatus(const QString &status) { if (m_status == status) return; m_status = status; emit statusChanged(); }
void UpdateService::setError(const QString &error) { if (m_error == error) return; m_error = error; emit errorChanged(); }

int UpdateService::compareVersions(const QString &left, const QString &right) {
  const auto a = versionParts(left); const auto b = versionParts(right);
  for (qsizetype i = 0; i < qMax(a.size(), b.size()); ++i) {
    const int av = i < a.size() ? a.at(i) : 0; const int bv = i < b.size() ? b.at(i) : 0;
    if (av != bv) return av < bv ? -1 : 1;
  }
  return 0;
}

QByteArray UpdateService::decodeSignature(const QByteArray &signature) {
  if (signature.size() == crypto_sign_BYTES) return signature;
  const auto trimmed = signature.trimmed();
  const auto hex = QByteArray::fromHex(trimmed); if (hex.size() == crypto_sign_BYTES) return hex;
  return QByteArray::fromBase64(trimmed);
}

bool UpdateService::verifyManifest(const QByteArray &manifest, const QByteArray &signature,
                                   const QByteArray &publicKey, QString *error) {
  if (sodium_init() < 0) { if (error) *error = QStringLiteral("The signature verifier could not initialize."); return false; }
  auto key = publicKey;
  if (key.size() != crypto_sign_PUBLICKEYBYTES) {
    const auto decodedHex = QByteArray::fromHex(publicKey.trimmed());
    key = decodedHex.size() == crypto_sign_PUBLICKEYBYTES ? decodedHex : QByteArray::fromBase64(publicKey.trimmed());
  }
  const auto decodedSignature = decodeSignature(signature);
  if (key.size() != crypto_sign_PUBLICKEYBYTES || decodedSignature.size() != crypto_sign_BYTES) {
    if (error) *error = QStringLiteral("The update signing key or signature has an invalid size."); return false;
  }
  const bool verified = crypto_sign_verify_detached(
    reinterpret_cast<const unsigned char *>(decodedSignature.constData()),
    reinterpret_cast<const unsigned char *>(manifest.constData()), static_cast<unsigned long long>(manifest.size()),
    reinterpret_cast<const unsigned char *>(key.constData())) == 0;
  if (!verified && error) *error = QStringLiteral("The release manifest signature is invalid.");
  return verified;
}

QVariantMap UpdateService::selectAsset(const QJsonObject &manifest, const QString &platform, const QString &architecture) {
  QVariantMap fallback;
  for (const auto &value : manifest.value(QStringLiteral("assets")).toArray()) {
    const auto asset = value.toObject();
    if (asset.value(QStringLiteral("platform")).toString() != platform || asset.value(QStringLiteral("architecture")).toString() != architecture) continue;
    const auto map = asset.toVariantMap();
    const auto kind = asset.value(QStringLiteral("kind")).toString();
    if ((platform == QStringLiteral("windows") && kind == QStringLiteral("installer")) ||
        (platform == QStringLiteral("macos") && kind == QStringLiteral("dmg")) ||
        (platform == QStringLiteral("linux") && kind == QStringLiteral("deb"))) return map;
    if (fallback.isEmpty()) fallback = map;
  }
  return fallback;
}

QString UpdateService::platformName() {
#if defined(Q_OS_WIN)
  return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
  return QStringLiteral("macos");
#else
  return QStringLiteral("linux");
#endif
}

QString UpdateService::architectureName() {
  const auto arch = QSysInfo::currentCpuArchitecture().toLower();
  return arch.contains(QStringLiteral("arm64")) || arch.contains(QStringLiteral("aarch64")) ? QStringLiteral("arm64") : QStringLiteral("x64");
}

void UpdateService::check() {
  if (m_checking) return;
  m_checking = true; emit checkingChanged(); setError({}); setStatus(QStringLiteral("checking"));
  QNetworkRequest request(releaseUrl(QStringLiteral("desktop-release-manifest.json")));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(20'000);
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    const auto body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
      setError(reply->errorString()); setStatus(QStringLiteral("unavailable")); m_checking = false; emit checkingChanged(); reply->deleteLater(); return;
    }
    reply->deleteLater(); fetchSignature(body);
  });
}

void UpdateService::fetchSignature(const QByteArray &manifest) {
  QNetworkRequest request(releaseUrl(QStringLiteral("desktop-release-manifest.json.sig")));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(20'000);
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, manifest] {
    const auto signature = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) { setError(reply->errorString()); setStatus(QStringLiteral("unavailable")); }
    else acceptManifest(manifest, signature);
    reply->deleteLater(); m_checking = false; emit checkingChanged();
  });
}

void UpdateService::acceptManifest(const QByteArray &manifest, const QByteArray &signature) {
  QString verificationError;
  if (!verifyManifest(manifest, signature, QByteArray(ANICLOUD_UPDATE_PUBLIC_KEY_HEX), &verificationError)) {
    setError(verificationError); setStatus(QStringLiteral("unverified")); return;
  }
  const auto doc = QJsonDocument::fromJson(manifest); const auto root = doc.object();
  if (!doc.isObject()) { setError(QStringLiteral("The signed manifest is malformed.")); setStatus(QStringLiteral("unverified")); return; }
  const auto version = root.value(QStringLiteral("version")).toString();
  if (compareVersions(version, QStringLiteral(ANICLOUD_VERSION)) <= 0) { m_asset.clear(); m_version.clear(); setStatus(QStringLiteral("current")); emit updateChanged(); return; }
  const auto asset = selectAsset(root, platformName(), architectureName());
  if (asset.isEmpty()) { setError(QStringLiteral("The signed release has no compatible asset.")); setStatus(QStringLiteral("unavailable")); return; }
  m_version = version; m_asset = asset; setStatus(QStringLiteral("required")); emit updateChanged();
}

void UpdateService::downloadAndInstall() {
  if (m_asset.isEmpty()) return;
  const QUrl url(m_asset.value(QStringLiteral("url")).toString());
  const auto fileName = QFileInfo(url.path()).fileName();
  const auto root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/updates/%1").arg(m_version);
  QDir().mkpath(root); m_installerPath = root + QLatin1Char('/') + fileName;
  auto *file = new QFile(m_installerPath + QStringLiteral(".part"), this);
  if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) { setError(file->errorString()); file->deleteLater(); return; }
  setStatus(QStringLiteral("downloading")); m_progress = 0; emit progressChanged();
  QNetworkRequest request(url); request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::readyRead, this, [reply, file] { file->write(reply->readAll()); });
  connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) { m_progress = total > 0 ? static_cast<double>(received) / total : 0; emit progressChanged(); });
  connect(reply, &QNetworkReply::finished, this, [this, reply, file] {
    file->write(reply->readAll()); file->flush(); file->close();
    if (reply->error() != QNetworkReply::NoError) { setError(reply->errorString()); setStatus(QStringLiteral("failed")); reply->deleteLater(); file->deleteLater(); return; }
    const auto part = file->fileName(); file->deleteLater(); reply->deleteLater();
    QFile downloaded(part); if (!downloaded.open(QIODevice::ReadOnly)) { setError(downloaded.errorString()); setStatus(QStringLiteral("failed")); return; }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!downloaded.atEnd()) hash.addData(downloaded.read(1024 * 1024));
    const auto size = downloaded.size(); downloaded.close();
    if (size != m_asset.value(QStringLiteral("size")).toLongLong() || QString::fromLatin1(hash.result().toHex()).compare(m_asset.value(QStringLiteral("sha256")).toString(), Qt::CaseInsensitive) != 0) {
      QFile::remove(part); setError(QStringLiteral("The downloaded update failed size or SHA-256 verification.")); setStatus(QStringLiteral("failed")); return;
    }
    QFile::remove(m_installerPath);
    if (!QFile::rename(part, m_installerPath)) { setError(QStringLiteral("Unable to finalize the verified installer.")); setStatus(QStringLiteral("failed")); return; }
    setStatus(QStringLiteral("ready")); launchInstaller();
  });
}

bool UpdateService::launchInstaller() {
  if (m_installerPath.isEmpty() || !QFileInfo::exists(m_installerPath)) {
    setError(QStringLiteral("The verified installer is no longer available. Download the update again."));
    setStatus(QStringLiteral("failed"));
    return false;
  }
  setError({});
  const bool launched = QProcess::startDetached(
    QCoreApplication::applicationFilePath(),
    {QStringLiteral("--update-installer-helper"), m_installerPath,
     QString::number(QCoreApplication::applicationPid())},
    QCoreApplication::applicationDirPath());
  if (!launched) {
    setError(QStringLiteral("The update helper could not be started. Keep AniCloud open and choose Launch installer to retry."));
    setStatus(QStringLiteral("ready"));
    return false;
  }
  setStatus(QStringLiteral("installing"));
  emit installerStarted();
  return true;
}

int UpdateService::runInstallerHelper(const QString &installerPath, qint64 parentProcessId) {
  if (installerPath.isEmpty() || !QFileInfo::exists(installerPath)) {
    QMessageBox::critical(nullptr, QStringLiteral("AniCloud update"),
                          QStringLiteral("The verified installer is missing. Reopen AniCloud and download the update again."));
    return 2;
  }

  while (processIsRunning(parentProcessId)) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 15'000 && processIsRunning(parentProcessId)) QThread::msleep(100);
    if (!processIsRunning(parentProcessId)) break;
    const auto choice = QMessageBox::warning(
      nullptr, QStringLiteral("Close AniCloud to update"),
      QStringLiteral("AniCloud is still running. Close it from the tray or Task Manager, then choose Retry."),
      QMessageBox::Retry | QMessageBox::Cancel, QMessageBox::Retry);
    if (choice != QMessageBox::Retry) return 3;
  }

  if (!launchInstallerDirect(installerPath)) {
    QMessageBox::critical(nullptr, QStringLiteral("AniCloud update"),
                          QStringLiteral("The installer could not be started. Reopen AniCloud and try the update again."));
    return 4;
  }
  return 0;
}
