#include "HlsGateway.h"

#include "HlsTools.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QSet>
#include <QTcpSocket>
#include <QUrlQuery>
#include <QUuid>

namespace {
constexpr int SessionLifetimeMinutes = 45;
constexpr qsizetype MaxRequestBytes = 32 * 1024;

QByteArray headerValue(const QList<QNetworkReply::RawHeaderPair> &headers, const QByteArray &name) {
  for (const auto &pair : headers) if (pair.first.compare(name, Qt::CaseInsensitive) == 0) return pair.second;
  return {};
}

bool needsPublicResolution(const QString &host) {
  const auto normalized = host.toLower();
  return normalized.endsWith(QStringLiteral(".tiktokcdn.com")) ||
         normalized.endsWith(QStringLiteral(".byteoversea.com")) ||
         normalized.endsWith(QStringLiteral(".ibyteimg.com"));
}

QString localResourceSuffix(const QUrl &upstream, const QString &resourceKind) {
  return QStringLiteral(".") + HlsTools::offlineExtension(upstream, resourceKind);
}

Qt::CaseSensitivity localPathCaseSensitivity() {
#if defined(Q_OS_WIN)
  return Qt::CaseInsensitive;
#else
  return Qt::CaseSensitive;
#endif
}

QString normalizedLocalPath(const QString &path) {
  return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

bool pathIsWithin(const QString &path, const QString &root) {
  const auto normalizedPath = normalizedLocalPath(path);
  auto normalizedRoot = normalizedLocalPath(root);
  if (normalizedPath.compare(normalizedRoot, localPathCaseSensitivity()) == 0) return true;
  if (!normalizedRoot.endsWith(QLatin1Char('/'))) normalizedRoot.append(QLatin1Char('/'));
  return normalizedPath.startsWith(normalizedRoot, localPathCaseSensitivity());
}

QByteArray localContentType(const QString &resourceId) {
  const auto extension = QFileInfo(resourceId).suffix().toLower();
  if (extension == QStringLiteral("m3u8")) return QByteArrayLiteral("application/vnd.apple.mpegurl");
  if (extension == QStringLiteral("vtt") || extension == QStringLiteral("srt")) return QByteArrayLiteral("text/vtt; charset=utf-8");
  if (extension == QStringLiteral("ts") || extension == QStringLiteral("m2ts") || extension == QStringLiteral("mts")) return QByteArrayLiteral("video/mp2t");
  if (extension == QStringLiteral("mp4")) return QByteArrayLiteral("video/mp4");
  if (extension == QStringLiteral("m4s")) return QByteArrayLiteral("video/iso.segment");
  if (extension == QStringLiteral("aac")) return QByteArrayLiteral("audio/aac");
  return QByteArrayLiteral("application/octet-stream");
}
}

HlsGateway::HlsGateway(QObject *parent) : QObject(parent) {
  connect(&m_server, &QTcpServer::newConnection, this, &HlsGateway::acceptConnection);
  m_expiryTimer.setInterval(60'000);
  connect(&m_expiryTimer, &QTimer::timeout, this, [this] {
    const auto now = QDateTime::currentDateTimeUtc();
    for (auto it = m_sessions.begin(); it != m_sessions.end();) {
      if (it->expiresAt < now) it = m_sessions.erase(it); else ++it;
    }
  });
  m_expiryTimer.start();
}

bool HlsGateway::ensureListening() {
  if (m_server.isListening()) return true;
  if (!m_server.listen(QHostAddress::LocalHost, 0)) {
    emit gatewayError(QStringLiteral("Unable to start the private playback gateway: %1").arg(m_server.errorString()));
    return false;
  }
  emit listeningChanged();
  return true;
}

QString HlsGateway::openSession(const QVariantMap &stream) {
  if (!ensureListening()) return {};
  const QUrl upstream(stream.value(QStringLiteral("mediaUrl")).toString());
  if (!upstream.isValid() || (upstream.scheme() != QStringLiteral("https") && upstream.scheme() != QStringLiteral("http"))) return {};
  const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
  Session session;
  session.headers = stream.value(QStringLiteral("headers")).toMap();
  session.subtitles = stream.value(QStringLiteral("subtitles")).toList();
  const auto referer = stream.value(QStringLiteral("referer")).toString();
  if (!referer.isEmpty()) session.headers.insert(QStringLiteral("Referer"), referer);
  session.expiresAt = QDateTime::currentDateTimeUtc().addSecs(SessionLifetimeMinutes * 60);
  m_sessions.insert(token, session);
  const auto result = localUrl(token, upstream, QStringLiteral("playlist"));
  const auto path = result.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (path.size() == 3) m_sessions[token].rootResourceId = path.at(2);
  return result.toString(QUrl::FullyEncoded);
}

QString HlsGateway::openOfflineSession(const QString &rootPath) {
  if (!ensureListening()) return {};
  const QFileInfo rootInfo(rootPath);
  const auto root = normalizedLocalPath(rootInfo.canonicalFilePath());
  if (!rootInfo.isDir() || root.isEmpty()) return {};
  const QFileInfo playlistInfo(QDir(root).absoluteFilePath(QStringLiteral("offline.m3u8")));
  const auto playlist = normalizedLocalPath(playlistInfo.canonicalFilePath());
  if (!playlistInfo.isFile() || playlist.isEmpty() || !pathIsWithin(playlist, root)) return {};

  const auto token = QUuid::createUuid().toString(QUuid::WithoutBraces).remove(QLatin1Char('-'));
  Session session;
  session.localRoot = root;
  session.expiresAt = QDateTime::currentDateTimeUtc().addSecs(SessionLifetimeMinutes * 60);
  m_sessions.insert(token, session);
  const auto result = localUrl(token, QUrl::fromLocalFile(playlist), QStringLiteral("playlist"));
  const auto path = result.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (path.size() != 3) { m_sessions.remove(token); return {}; }
  m_sessions[token].rootResourceId = path.at(2);
  return result.toString(QUrl::FullyEncoded);
}

void HlsGateway::closeSession(const QString &sessionId) { m_sessions.remove(sessionId); }

void HlsGateway::closeAll() { m_sessions.clear(); }

QUrl HlsGateway::localUrl(const QString &token, const QUrl &upstream, const QString &resourceKind) {
  auto it = m_sessions.find(token);
  if (it == m_sessions.end()) return {};
  auto source = upstream;
  if (!it->localRoot.isEmpty()) {
    if (!source.isLocalFile()) return {};
    const QFileInfo localInfo(source.toLocalFile());
    const auto canonical = normalizedLocalPath(localInfo.canonicalFilePath());
    if (!localInfo.isFile() || canonical.isEmpty() || !pathIsWithin(canonical, it->localRoot)) return {};
    source = QUrl::fromLocalFile(canonical);
  }
  const auto normalized = source.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
  // A direct media playlist needs a second opaque URL so the root URL can serve
  // a synthetic master. Other resources retain URL-based deduplication so the
  // kind discovered while parsing (for example a disguised .ts segment) wins.
  const auto identifier = normalized + (resourceKind.compare(QStringLiteral("media"), Qt::CaseInsensitive) == 0
    ? QStringLiteral("|media") : QString{});
  auto id = it->identifiers.value(identifier);
  if (id.isEmpty()) {
    const auto baseId = QString::fromLatin1(QCryptographicHash::hash(identifier.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    const auto suffix = localResourceSuffix(source, resourceKind);
    id = baseId + suffix;
    int collision = 0;
    while (it->resources.contains(id) && it->resources.value(id) != source)
      id = baseId + QStringLiteral("-%1").arg(++collision) + suffix;
    it->resources.insert(id, source);
    it->identifiers.insert(identifier, id);
  }
  return QUrl(QStringLiteral("http://127.0.0.1:%1/s/%2/%3").arg(m_server.serverPort()).arg(token, id));
}

void HlsGateway::acceptConnection() {
  while (m_server.hasPendingConnections()) {
    auto *socket = m_server.nextPendingConnection();
    socket->setParent(this);
    connect(socket, &QTcpSocket::readyRead, this, [this, socket] { readRequest(socket); });
    connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
  }
}

void HlsGateway::readRequest(QTcpSocket *socket) {
  auto buffer = socket->property("requestBuffer").toByteArray() + socket->readAll();
  if (buffer.size() > MaxRequestBytes) { sendError(socket, 431, QByteArrayLiteral("Request Header Fields Too Large")); return; }
  const auto boundary = buffer.indexOf(QByteArrayLiteral("\r\n\r\n"));
  if (boundary < 0) { socket->setProperty("requestBuffer", buffer); return; }
  socket->setProperty("requestBuffer", {});
  const auto headerLines = buffer.left(boundary).split('\n');
  if (headerLines.isEmpty()) { sendError(socket, 400, QByteArrayLiteral("Bad Request")); return; }
  const auto requestParts = headerLines.first().trimmed().split(' ');
  if (requestParts.size() < 2 || (requestParts.at(0) != QByteArrayLiteral("GET") && requestParts.at(0) != QByteArrayLiteral("HEAD"))) {
    sendError(socket, 405, QByteArrayLiteral("Method Not Allowed")); return;
  }
  const auto path = QUrl::fromEncoded(requestParts.at(1)).path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (path.size() != 3 || path.at(0) != QStringLiteral("s")) { sendError(socket, 404, QByteArrayLiteral("Not Found")); return; }
  QHash<QByteArray, QByteArray> headers;
  for (qsizetype i = 1; i < headerLines.size(); ++i) {
    const auto line = headerLines.at(i).trimmed(); const auto colon = line.indexOf(':');
    if (colon > 0) headers.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
  }
  QObject::disconnect(socket, nullptr, this, nullptr);
  proxy(socket, requestParts.at(0), path.at(1), path.at(2), headers);
}

void HlsGateway::proxy(QTcpSocket *socket, const QByteArray &method, const QString &token,
                       const QString &resourceId, const QHash<QByteArray, QByteArray> &incomingHeaders) {
  auto it = m_sessions.find(token);
  if (it == m_sessions.end() || it->expiresAt < QDateTime::currentDateTimeUtc()) { sendError(socket, 410, QByteArrayLiteral("Gone")); return; }
  const auto upstream = it->resources.value(resourceId);
  if (!upstream.isValid()) { sendError(socket, 404, QByteArrayLiteral("Not Found")); return; }
  it->expiresAt = QDateTime::currentDateTimeUtc().addSecs(SessionLifetimeMinutes * 60);
  if (upstream.isLocalFile()) {
    serveLocal(socket, method, token, resourceId, incomingHeaders);
    return;
  }
  if (!it->localRoot.isEmpty()) { sendError(socket, 404, QByteArrayLiteral("Not Found")); return; }
  if (needsPublicResolution(upstream.host())) {
    const QPointer<QTcpSocket> guardedSocket(socket);
    resolvePublicAddress(upstream.host(), [this, guardedSocket, method, token, resourceId, incomingHeaders](const QString &address) {
      if (guardedSocket && guardedSocket->state() != QAbstractSocket::UnconnectedState)
        proxyResolved(guardedSocket, method, token, resourceId, incomingHeaders, address);
    });
    return;
  }
  proxyResolved(socket, method, token, resourceId, incomingHeaders, {});
}

void HlsGateway::serveLocal(QTcpSocket *socket, const QByteArray &method, const QString &token,
                            const QString &resourceId,
                            const QHash<QByteArray, QByteArray> &incomingHeaders) {
  auto session = m_sessions.find(token);
  if (session == m_sessions.end() || session->localRoot.isEmpty()) {
    sendError(socket, 410, QByteArrayLiteral("Gone")); return;
  }
  const auto source = session->resources.value(resourceId);
  const QFileInfo sourceInfo(source.toLocalFile());
  const auto canonical = normalizedLocalPath(sourceInfo.canonicalFilePath());
  if (!source.isLocalFile() || !sourceInfo.isFile() || canonical.isEmpty() ||
      !pathIsWithin(canonical, session->localRoot)) {
    sendError(socket, 404, QByteArrayLiteral("Not Found")); return;
  }

  QFile file(canonical);
  if (!file.open(QIODevice::ReadOnly)) { sendError(socket, 404, QByteArrayLiteral("Not Found")); return; }
  QByteArray body;
  auto contentType = localContentType(resourceId);
  if (resourceId.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive)) {
    body = file.readAll();
    const auto baseUrl = QUrl::fromLocalFile(canonical);
    bool validManifest = true;
    for (const auto &resource : HlsTools::resources(body, baseUrl))
      if (localUrl(token, resource.url, resource.kind).isEmpty()) validManifest = false;
    if (validManifest) {
      body = HlsTools::rewrite(body, baseUrl, [this, token, &validManifest](const QUrl &url) {
        const auto mapped = localUrl(token, url);
        if (mapped.isEmpty()) validManifest = false;
        return mapped;
      });
    }
    if (!validManifest) { sendError(socket, 403, QByteArrayLiteral("Forbidden")); return; }
  }

  qint64 start = 0;
  qint64 end = resourceId.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive)
    ? body.size() - 1 : file.size() - 1;
  int status = 200;
  const auto range = incomingHeaders.value(QByteArrayLiteral("range"));
  if (!range.isEmpty() && !resourceId.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive)) {
    const QRegularExpression expression(QStringLiteral("^bytes=(\\d*)-(\\d*)$"));
    const auto match = expression.match(QString::fromLatin1(range).trimmed());
    bool startOk = false;
    bool endOk = false;
    const auto requestedStart = match.hasMatch() ? match.captured(1).toLongLong(&startOk) : 0;
    const auto requestedEnd = match.hasMatch() ? match.captured(2).toLongLong(&endOk) : 0;
    if (!match.hasMatch() || (!startOk && !endOk) || file.size() <= 0) {
      sendError(socket, 416, QByteArrayLiteral("Range Not Satisfiable")); return;
    }
    if (startOk) {
      start = requestedStart;
      end = endOk ? requestedEnd : file.size() - 1;
    } else {
      const auto suffixLength = qMin(requestedEnd, file.size());
      start = file.size() - suffixLength;
      end = file.size() - 1;
    }
    if (start < 0 || start >= file.size() || end < start) {
      sendError(socket, 416, QByteArrayLiteral("Range Not Satisfiable")); return;
    }
    end = qMin(end, file.size() - 1);
    status = 206;
  }

  const auto contentLength = qMax<qint64>(0, end - start + 1);
  if (method != QByteArrayLiteral("HEAD") && !resourceId.endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive)) {
    if (!file.seek(start)) { sendError(socket, 500, QByteArrayLiteral("Internal Server Error")); return; }
    body = file.read(contentLength);
    if (body.size() != contentLength) { sendError(socket, 500, QByteArrayLiteral("Internal Server Error")); return; }
  }

  QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + QByteArrayLiteral(" ") + reasonFor(status) + QByteArrayLiteral("\r\n");
  response += QByteArrayLiteral("Connection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\nAccept-Ranges: bytes\r\n");
  response += QByteArrayLiteral("Content-Type: ") + contentType + QByteArrayLiteral("\r\n");
  if (status == 206)
    response += QByteArrayLiteral("Content-Range: bytes ") + QByteArray::number(start) + QByteArrayLiteral("-") + QByteArray::number(end) + QByteArrayLiteral("/") + QByteArray::number(file.size()) + QByteArrayLiteral("\r\n");
  response += QByteArrayLiteral("Content-Length: ") + QByteArray::number(contentLength) + QByteArrayLiteral("\r\n\r\n");
  socket->write(response);
  if (method != QByteArrayLiteral("HEAD")) socket->write(body);
  socket->disconnectFromHost();
}

void HlsGateway::proxyResolved(QTcpSocket *socket, const QByteArray &method, const QString &token,
                               const QString &resourceId, const QHash<QByteArray, QByteArray> &incomingHeaders,
                               const QString &publicAddress) {
  auto it = m_sessions.find(token);
  if (it == m_sessions.end()) { sendError(socket, 410, QByteArrayLiteral("Gone")); return; }
  const auto upstream = it->resources.value(resourceId);
  if (!upstream.isValid()) { sendError(socket, 404, QByteArrayLiteral("Not Found")); return; }
  auto routedUrl = upstream;
  if (!publicAddress.isEmpty()) routedUrl.setHost(publicAddress);
  QNetworkRequest request(routedUrl);
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
  request.setTransferTimeout(30'000);
  for (auto header = it->headers.cbegin(); header != it->headers.cend(); ++header)
    if (!header.value().toString().isEmpty()) request.setRawHeader(header.key().toUtf8(), header.value().toString().toUtf8());
  if (!publicAddress.isEmpty()) {
    request.setPeerVerifyName(upstream.host());
    auto hostHeader = upstream.host().toUtf8();
    if (upstream.port() > 0 && upstream.port() != 443) hostHeader += QByteArrayLiteral(":") + QByteArray::number(upstream.port());
    request.setRawHeader(QByteArrayLiteral("Host"), hostHeader);
  }
  if (!request.hasRawHeader(QByteArrayLiteral("User-Agent")))
    request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArrayLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/148.0.0.0 Safari/537.36"));
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("*/*"));
  request.setRawHeader(QByteArrayLiteral("Accept-Encoding"), QByteArrayLiteral("identity"));
  if (incomingHeaders.contains(QByteArrayLiteral("range"))) request.setRawHeader(QByteArrayLiteral("Range"), incomingHeaders.value(QByteArrayLiteral("range")));
  auto *reply = method == QByteArrayLiteral("HEAD") ? m_network.head(request) : m_network.get(request);
  connect(socket, &QTcpSocket::disconnected, reply, &QNetworkReply::abort);
  connect(reply, &QNetworkReply::finished, this, [this, reply, socket, method, token, upstream, resourceId] {
    if (socket->state() == QAbstractSocket::UnconnectedState) { reply->deleteLater(); return; }
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 400) {
      sendError(socket, status >= 400 ? status : 502, reasonFor(status >= 400 ? status : 502)); reply->deleteLater(); return;
    }
    auto body = method == QByteArrayLiteral("HEAD") ? QByteArray{} : reply->readAll();
    auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toUtf8();
    const auto finalUrl = reply->url().isValid() ? reply->url() : upstream;
    if (method != QByteArrayLiteral("HEAD") && HlsTools::looksLikePlaylist(body, finalUrl)) {
      for (const auto &resource : HlsTools::resources(body, finalUrl))
        localUrl(token, resource.url, resource.kind);
      body = HlsTools::rewrite(body, finalUrl, [this, token](const QUrl &url) { return localUrl(token, url); });
      const auto session = m_sessions.constFind(token);
      if (session != m_sessions.cend() && session->rootResourceId == resourceId && !session->subtitles.isEmpty()) {
        QList<QPair<QString, QUrl>> captions;
        for (const auto &value : session->subtitles) {
          const auto track = value.toMap();
          const QUrl url(track.value(QStringLiteral("url"), track.value(QStringLiteral("file"))).toString());
          if (url.isValid()) captions.append({track.value(QStringLiteral("label"), QStringLiteral("Captions")).toString(), localUrl(token, url, QStringLiteral("subtitle"))});
        }
        if (body.contains("#EXT-X-STREAM-INF")) {
          body = HlsTools::addSubtitleTracks(body, captions);
        } else {
          const auto mediaUrl = localUrl(token, finalUrl, QStringLiteral("media"));
          body = HlsTools::makeOfflineMaster(mediaUrl, captions);
        }
      }
      contentType = QByteArrayLiteral("application/vnd.apple.mpegurl");
    }
    if (resourceId.endsWith(QStringLiteral(".vtt"), Qt::CaseInsensitive))
      contentType = QByteArrayLiteral("text/vtt; charset=utf-8");
    QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + QByteArrayLiteral(" ") + reasonFor(status) + QByteArrayLiteral("\r\n");
    response += QByteArrayLiteral("Connection: close\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *\r\n");
    if (!contentType.isEmpty()) response += QByteArrayLiteral("Content-Type: ") + contentType + QByteArrayLiteral("\r\n");
    for (const auto &name : {QByteArrayLiteral("Content-Range"), QByteArrayLiteral("Accept-Ranges"), QByteArrayLiteral("Last-Modified")}) {
      const auto value = headerValue(reply->rawHeaderPairs(), name); if (!value.isEmpty()) response += name + QByteArrayLiteral(": ") + value + QByteArrayLiteral("\r\n");
    }
    const auto upstreamLength = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    const auto responseLength = method == QByteArrayLiteral("HEAD") && upstreamLength >= 0 ? upstreamLength : body.size();
    response += QByteArrayLiteral("Content-Length: ") + QByteArray::number(responseLength) + QByteArrayLiteral("\r\n\r\n");
    socket->write(response);
    if (method != QByteArrayLiteral("HEAD")) socket->write(body);
    socket->disconnectFromHost(); reply->deleteLater();
  });
}

void HlsGateway::resolvePublicAddress(const QString &host, std::function<void(const QString &)> callback) {
  if (m_publicAddresses.contains(host)) { callback(m_publicAddresses.value(host)); return; }
  m_dnsWaiters[host].append(std::move(callback));
  if (m_dnsWaiters.value(host).size() > 1) return;
  QUrl url(QStringLiteral("https://cloudflare-dns.com/dns-query"));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("name"), host);
  query.addQueryItem(QStringLiteral("type"), QStringLiteral("A"));
  url.setQuery(query);
  QNetworkRequest request(url);
  request.setTransferTimeout(8'000);
  request.setRawHeader(QByteArrayLiteral("Accept"), QByteArrayLiteral("application/dns-json"));
  request.setRawHeader(QByteArrayLiteral("User-Agent"), QByteArrayLiteral("AniCloudDesktop/4.0"));
  auto *reply = m_network.get(request);
  connect(reply, &QNetworkReply::finished, this, [this, reply, host] {
    QString address;
    const auto root = QJsonDocument::fromJson(reply->readAll()).object();
    if (reply->error() == QNetworkReply::NoError) {
      for (const auto &value : root.value(QStringLiteral("Answer")).toArray()) {
        const auto answer = value.toObject();
        QHostAddress candidate(answer.value(QStringLiteral("data")).toString());
        if (answer.value(QStringLiteral("type")).toInt() == 1 && candidate.protocol() == QAbstractSocket::IPv4Protocol) {
          address = candidate.toString();
          break;
        }
      }
    }
    if (!address.isEmpty()) m_publicAddresses.insert(host, address);
    const auto callbacks = m_dnsWaiters.take(host);
    for (const auto &waiting : callbacks) waiting(address);
    reply->deleteLater();
  });
}

void HlsGateway::sendError(QTcpSocket *socket, int status, const QByteArray &reason) {
  const auto body = QByteArray::number(status) + QByteArrayLiteral(" ") + reason + QByteArrayLiteral("\n");
  socket->write(QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) + QByteArrayLiteral(" ") + reason +
                QByteArrayLiteral("\r\nConnection: close\r\nContent-Type: text/plain\r\nContent-Length: ") + QByteArray::number(body.size()) + QByteArrayLiteral("\r\n\r\n") + body);
  socket->disconnectFromHost();
}

QByteArray HlsGateway::reasonFor(int status) {
  switch (status) {
    case 200: return QByteArrayLiteral("OK");
    case 206: return QByteArrayLiteral("Partial Content");
    case 400: return QByteArrayLiteral("Bad Request");
    case 403: return QByteArrayLiteral("Forbidden");
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 410: return QByteArrayLiteral("Gone");
    case 416: return QByteArrayLiteral("Range Not Satisfiable");
    case 431: return QByteArrayLiteral("Request Header Fields Too Large");
    case 500: return QByteArrayLiteral("Internal Server Error");
    case 502: return QByteArrayLiteral("Bad Gateway");
    default: return QByteArrayLiteral("Upstream Response");
  }
}
