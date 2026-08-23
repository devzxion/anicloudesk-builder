#include "HlsGateway.h"

#include <QHash>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

class HlsGatewayTest final : public QObject {
  Q_OBJECT

  struct Response {
    int status = 0;
    QByteArray body;
    QByteArray contentRange;
    qint64 contentLength = -1;
  };

private slots:
  void initTestCase() {
    QVERIFY(m_upstream.listen(QHostAddress::LocalHost, 0));
    connect(&m_upstream, &QTcpServer::newConnection, this, [this] {
      while (m_upstream.hasPendingConnections()) {
        auto *socket = m_upstream.nextPendingConnection();
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] { serve(socket); });
        connect(socket, &QTcpSocket::disconnected, socket, &QTcpSocket::deleteLater);
      }
    });
  }

  void rewritesRedirectedManifestAndInjectsHeaders() {
    HlsGateway gateway;
    const auto local = gateway.openSession({
      {QStringLiteral("mediaUrl"), upstreamUrl(QStringLiteral("/redirect.m3u8")).toString()},
      {QStringLiteral("referer"), QStringLiteral("https://embed.example/watch/1")},
    });
    QVERIFY(local.startsWith(QStringLiteral("http://127.0.0.1:")));

    const auto manifest = request(QUrl(local));
    QCOMPARE(manifest.status, 200);
    QVERIFY(QUrl(local).path().endsWith(QStringLiteral(".m3u8")));
    QVERIFY(manifest.body.startsWith("#EXTM3U"));
    QVERIFY(!manifest.body.contains("127.0.0.1:") || !manifest.body.contains(QByteArray::number(m_upstream.serverPort())));
    const auto segmentLine = manifest.body.split('\n').last().trimmed();
    const QUrl segmentUrl(QString::fromUtf8(segmentLine));
    QVERIFY(segmentUrl.isValid());
    QVERIFY(segmentUrl.path().startsWith(QStringLiteral("/s/")));
    QVERIFY(segmentUrl.path().endsWith(QStringLiteral(".ts")));

    QNetworkRequest rangedRequest(segmentUrl);
    rangedRequest.setRawHeader(QByteArrayLiteral("Range"), QByteArrayLiteral("bytes=1-2"));
    const auto segment = request(rangedRequest);
    QCOMPARE(segment.status, 206);
    QCOMPARE(segment.body, QByteArrayLiteral("BC"));
    QCOMPARE(segment.contentRange, QByteArrayLiteral("bytes 1-2/4"));
    QCOMPARE(m_lastHeaders.value(QByteArrayLiteral("referer")), QByteArrayLiteral("https://embed.example/watch/1"));
    QVERIFY(m_lastHeaders.value(QByteArrayLiteral("origin")).isEmpty());
    QVERIFY(m_lastHeaders.value(QByteArrayLiteral("user-agent")).startsWith("Mozilla/"));

    const auto head = request(QNetworkRequest(segmentUrl), true);
    QCOMPARE(head.status, 200);
    QCOMPARE(head.body.size(), 0);
    QCOMPARE(head.contentLength, 4);
  }

  void rejectsUnknownResourcesAndClosedSessions() {
    HlsGateway gateway;
    const auto local = QUrl(gateway.openSession({{QStringLiteral("mediaUrl"), upstreamUrl(QStringLiteral("/master.m3u8")).toString()}}));
    QVERIFY(local.isValid());
    const auto parts = local.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QCOMPARE(parts.size(), 3);

    auto unknown = local;
    unknown.setPath(QStringLiteral("/s/%1/not-a-resource").arg(parts.at(1)));
    QCOMPARE(request(unknown).status, 404);

    gateway.closeSession(parts.at(1));
    QCOMPARE(request(local).status, 410);
  }

  void injectsExternalCaptionsIntoAdaptiveMasters() {
    HlsGateway gateway;
    const auto local = gateway.openSession({
      {QStringLiteral("mediaUrl"), upstreamUrl(QStringLiteral("/adaptive.m3u8")).toString()},
      {QStringLiteral("subtitles"), QVariantList{QVariantMap{
        {QStringLiteral("label"), QStringLiteral("English")},
        {QStringLiteral("url"), upstreamUrl(QStringLiteral("/en.vtt")).toString()},
      }}},
    });
    const auto manifest = request(QUrl(local));
    QCOMPARE(manifest.status, 200);
    QVERIFY(manifest.body.contains("#EXT-X-MEDIA:TYPE=SUBTITLES"));
    QVERIFY(manifest.body.contains("NAME=\"English\""));
    QVERIFY(manifest.body.contains("SUBTITLES=\"anicloud-subs\""));
    QVERIFY(manifest.body.contains(".vtt\""));
    QVERIFY(!manifest.body.contains(QByteArray::number(m_upstream.serverPort())));
  }

  void wrapsDirectMediaPlaylistsSoExternalCaptionsRemainSelectable() {
    HlsGateway gateway;
    const auto local = gateway.openSession({
      {QStringLiteral("mediaUrl"), upstreamUrl(QStringLiteral("/master.m3u8")).toString()},
      {QStringLiteral("subtitles"), QVariantList{QVariantMap{
        {QStringLiteral("label"), QStringLiteral("English")},
        {QStringLiteral("url"), upstreamUrl(QStringLiteral("/en.vtt")).toString()},
      }}},
    });
    const auto master = request(QUrl(local));
    QCOMPARE(master.status, 200);
    QVERIFY(master.body.contains("#EXT-X-MEDIA:TYPE=SUBTITLES"));
    QVERIFY(master.body.contains("#EXT-X-STREAM-INF"));

    const auto lines = master.body.split('\n');
    const QUrl mediaUrl(QString::fromUtf8(lines.last().trimmed()));
    QVERIFY(mediaUrl.isValid());
    QVERIFY(mediaUrl != QUrl(local));
    const auto media = request(mediaUrl);
    QCOMPARE(media.status, 200);
    QVERIFY(media.body.contains("#EXTINF"));

    const QRegularExpression captionExpression(QStringLiteral("URI=\\\"([^\\\"]+)\\\""));
    const auto captionMatch = captionExpression.match(QString::fromUtf8(master.body));
    QVERIFY(captionMatch.hasMatch());
    const auto captions = request(QUrl(captionMatch.captured(1)));
    QCOMPARE(captions.status, 200);
    QVERIFY(captions.body.startsWith("WEBVTT"));
  }

  void assignsTransportStreamSuffixToDisguisedSegments() {
    HlsGateway gateway;
    const auto local = QUrl(gateway.openSession({
      {QStringLiteral("mediaUrl"), upstreamUrl(QStringLiteral("/fake-extension.m3u8")).toString()},
    }));
    const auto manifest = request(local);
    QCOMPARE(manifest.status, 200);
    const QUrl segmentUrl(QString::fromUtf8(manifest.body.split('\n').last().trimmed()));
    QVERIFY(segmentUrl.isValid());
    QVERIFY(segmentUrl.path().endsWith(QStringLiteral(".ts")));
    const auto segment = request(segmentUrl);
    QCOMPARE(segment.status, 200);
    QCOMPARE(segment.body, QByteArrayLiteral("FAKE-TS"));
  }

private:
  QUrl upstreamUrl(const QString &path) const {
    return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(m_upstream.serverPort()).arg(path));
  }

  Response request(const QUrl &url, bool head = false) {
    return request(QNetworkRequest(url), head);
  }

  Response request(const QNetworkRequest &request, bool head = false) {
    QNetworkAccessManager manager;
    auto *reply = head ? manager.head(request) : manager.get(request);
    QSignalSpy finished(reply, &QNetworkReply::finished);
    if (!reply->isFinished() && !finished.wait(3000)) {
      QTest::qFail("Timed out waiting for the loopback gateway", __FILE__, __LINE__);
      return {};
    }
    Response response;
    response.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    response.body = reply->readAll();
    response.contentRange = reply->rawHeader(QByteArrayLiteral("Content-Range"));
    response.contentLength = reply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    reply->deleteLater();
    return response;
  }

  void serve(QTcpSocket *socket) {
    auto request = socket->property("request").toByteArray() + socket->readAll();
    if (!request.contains("\r\n\r\n")) { socket->setProperty("request", request); return; }
    const auto lines = request.split('\n');
    const auto first = lines.first().trimmed().split(' ');
    if (first.size() < 2) { socket->disconnectFromHost(); return; }
    const auto method = first.at(0);
    const auto path = QUrl::fromEncoded(first.at(1)).path();
    m_lastHeaders.clear();
    for (qsizetype index = 1; index < lines.size(); ++index) {
      const auto line = lines.at(index).trimmed();
      const auto colon = line.indexOf(':');
      if (colon > 0) m_lastHeaders.insert(line.left(colon).trimmed().toLower(), line.mid(colon + 1).trimmed());
    }

    if (path == QStringLiteral("/redirect.m3u8")) {
      socket->write("HTTP/1.1 302 Found\r\nLocation: /master.m3u8\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
      socket->disconnectFromHost(); return;
    }
    if (path == QStringLiteral("/master.m3u8")) {
      const QByteArray body("#EXTM3U\n#EXTINF:5,\nsegment.ts");
      writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : body, body.size(), {});
      return;
    }
    if (path == QStringLiteral("/adaptive.m3u8")) {
      const QByteArray body("#EXTM3U\n#EXT-X-STREAM-INF:BANDWIDTH=1000000\n/master.m3u8");
      writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : body, body.size(), {});
      return;
    }
    if (path == QStringLiteral("/fake-extension.m3u8")) {
      const QByteArray body("#EXTM3U\n#EXTINF:5,\n/disguised.jpg");
      writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : body, body.size(), {});
      return;
    }
    if (path == QStringLiteral("/en.vtt")) {
      const QByteArray body("WEBVTT\n\n00:00.000 --> 00:01.000\nHello");
      writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : body, body.size(), {});
      return;
    }
    if (path == QStringLiteral("/segment.ts")) {
      const QByteArray full("ABCD");
      if (m_lastHeaders.value(QByteArrayLiteral("range")) == QByteArrayLiteral("bytes=1-2")) {
        writeResponse(socket, 206, method == QByteArrayLiteral("HEAD") ? QByteArray{} : QByteArrayLiteral("BC"), 2, QByteArrayLiteral("bytes 1-2/4"));
      } else {
        writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : full, full.size(), {});
      }
      return;
    }
    if (path == QStringLiteral("/disguised.jpg")) {
      const QByteArray full("FAKE-TS");
      writeResponse(socket, 200, method == QByteArrayLiteral("HEAD") ? QByteArray{} : full, full.size(), {});
      return;
    }
    writeResponse(socket, 404, QByteArrayLiteral("missing"), 7, {});
  }

  static void writeResponse(QTcpSocket *socket, int status, const QByteArray &body, qint64 length, const QByteArray &range) {
    QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(status) +
      (status == 206 ? QByteArrayLiteral(" Partial Content\r\n") : status == 200 ? QByteArrayLiteral(" OK\r\n") : QByteArrayLiteral(" Not Found\r\n"));
    response += QByteArrayLiteral("Content-Type: ") + (body.startsWith("#EXTM3U") ? QByteArrayLiteral("application/vnd.apple.mpegurl") : QByteArrayLiteral("application/octet-stream")) + QByteArrayLiteral("\r\n");
    if (!range.isEmpty()) response += QByteArrayLiteral("Content-Range: ") + range + QByteArrayLiteral("\r\nAccept-Ranges: bytes\r\n");
    response += QByteArrayLiteral("Content-Length: ") + QByteArray::number(length) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n") + body;
    socket->write(response); socket->disconnectFromHost();
  }

  QTcpServer m_upstream;
  QHash<QByteArray, QByteArray> m_lastHeaders;
};

QTEST_GUILESS_MAIN(HlsGatewayTest)
#include "test_hlsgateway.moc"
