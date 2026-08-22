#pragma once

#include <QDateTime>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTcpServer>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class QTcpSocket;

class HlsGateway final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool listening READ listening NOTIFY listeningChanged)
  Q_PROPERTY(quint16 port READ port NOTIFY listeningChanged)

public:
  explicit HlsGateway(QObject *parent = nullptr);

  [[nodiscard]] bool listening() const { return m_server.isListening(); }
  [[nodiscard]] quint16 port() const { return m_server.serverPort(); }

  Q_INVOKABLE QString openSession(const QVariantMap &stream);
  Q_INVOKABLE void closeSession(const QString &sessionId);
  Q_INVOKABLE void closeAll();

signals:
  void listeningChanged();
  void gatewayError(const QString &message);

private:
  struct Session {
    QVariantMap headers;
    QVariantList subtitles;
    QHash<QString, QUrl> resources;
    QHash<QString, QString> identifiers;
    QString rootResourceId;
    QDateTime expiresAt;
  };

  bool ensureListening();
  void acceptConnection();
  void readRequest(QTcpSocket *socket);
  void proxy(QTcpSocket *socket, const QByteArray &method, const QString &token,
             const QString &resourceId, const QHash<QByteArray, QByteArray> &incomingHeaders);
  void proxyResolved(QTcpSocket *socket, const QByteArray &method, const QString &token,
                     const QString &resourceId, const QHash<QByteArray, QByteArray> &incomingHeaders,
                     const QString &publicAddress);
  void resolvePublicAddress(const QString &host, std::function<void(const QString &)> callback);
  QUrl localUrl(const QString &token, const QUrl &upstream, const QString &resourceKind = {});
  void sendError(QTcpSocket *socket, int status, const QByteArray &reason);
  static QByteArray reasonFor(int status);

  QTcpServer m_server;
  QNetworkAccessManager m_network;
  QHash<QString, Session> m_sessions;
  QHash<QString, QString> m_publicAddresses;
  QHash<QString, QList<std::function<void(const QString &)>>> m_dnsWaiters;
  QTimer m_expiryTimer;
};
