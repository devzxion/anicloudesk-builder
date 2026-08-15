#include "HlsGateway.h"

#include "HlsTools.h"

#include <QCryptographicHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
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
  const auto result = localUrl(token, upstream);
  const auto path = result.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (path.size() == 3) m_sessions[token].rootResourceId = path.at(2);
  return result.toString(QUrl::FullyEncoded);
}

void HlsGateway::closeSession(const QString &sessionId) { m_sessions.remove(sessionId); }

void HlsGateway::closeAll() { m_sessions.clear(); }

QUrl HlsGateway::localUrl(const QString &token, const QUrl &upstream) {
  auto it = m_sessions.find(token);
  if (it == m_sessions.end()) return {};
  const auto normalized = upstream.adjusted(QUrl::NormalizePathSegments | QUrl::RemoveFragment).toString(QUrl::FullyEncoded);
  auto id = it->identifiers.value(normalized);
  if (id.isEmpty()) {
    id = QString::fromLatin1(QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
    int suffix = 0;
    while (it->resources.contains(id) && it->resources.value(id) != upstream) id = id.section(QLatin1Char('-'), 0, 0) + QStringLiteral("-%1").arg(++suffix);
    it->resources.insert(id, upstream);
    it->identifiers.insert(normalized, id);
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
  connect(reply, &QNetworkReply::finished, this, [this, reply, socket, method, token, upstream] {
    if (socket->state() == QAbstractSocket::UnconnectedState) { reply->deleteLater(); return; }
    const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (reply->error() != QNetworkReply::NoError || status < 200 || status >= 400) {
      sendError(socket, status >= 400 ? status : 502, reasonFor(status >= 400 ? status : 502)); reply->deleteLater(); return;
    }
    auto body = method == QByteArrayLiteral("HEAD") ? QByteArray{} : reply->readAll();
    auto contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString().toUtf8();
    const auto finalUrl = reply->url().isValid() ? reply->url() : upstream;
    if (method != QByteArrayLiteral("HEAD") && HlsTools::looksLikePlaylist(body, finalUrl)) {
      body = HlsTools::rewrite(body, finalUrl, [this, token](const QUrl &url) { return localUrl(token, url); });
      const auto session = m_sessions.constFind(token);
      if (session != m_sessions.cend() && session->rootResourceId == resourceId && !session->subtitles.isEmpty()) {
        QList<QPair<QString, QUrl>> captions;
        for (const auto &value : session->subtitles) {
          const auto track = value.toMap();
          const QUrl url(track.value(QStringLiteral("url"), track.value(QStringLiteral("file"))).toString());
          if (url.isValid()) captions.append({track.value(QStringLiteral("label"), QStringLiteral("Captions")).toString(), localUrl(token, url)});
        }
        body = HlsTools::addSubtitleTracks(body, captions);
      }
      contentType = QByteArrayLiteral("application/vnd.apple.mpegurl");
    }
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
    case 404: return QByteArrayLiteral("Not Found");
    case 405: return QByteArrayLiteral("Method Not Allowed");
    case 410: return QByteArrayLiteral("Gone");
    case 416: return QByteArrayLiteral("Range Not Satisfiable");
    case 431: return QByteArrayLiteral("Request Header Fields Too Large");
    case 502: return QByteArrayLiteral("Bad Gateway");
    default: return QByteArrayLiteral("Upstream Response");
  }
}
