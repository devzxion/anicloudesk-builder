#include "ApiClient.h"
#include "AppRuntime.h"
#include "BuildConfig.h"
#include "Database.h"
#include "DownloadManager.h"
#include "HlsGateway.h"
#include "PlayerController.h"
#include "SecureStore.h"
#include "UpdateService.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFileOpenEvent>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QNetworkInformation>
#include <QTimer>

namespace {
class DeepLinkEventFilter final : public QObject {
public:
  explicit DeepLinkEventFilter(AppRuntime *runtime) : m_runtime(runtime) {}
protected:
  bool eventFilter(QObject *watched, QEvent *event) override {
    if (event->type() == QEvent::FileOpen) {
      const auto *openEvent = static_cast<QFileOpenEvent *>(event);
      m_runtime->handleDeepLink(openEvent->url().toString());
      return true;
    }
    return QObject::eventFilter(watched, event);
  }
private:
  AppRuntime *m_runtime;
};
}

int main(int argc, char *argv[]) {
  QApplication::setOrganizationName(QStringLiteral("AniCloud"));
  QApplication::setOrganizationDomain(QStringLiteral("anicloud.ink"));
  QApplication::setApplicationName(QStringLiteral("AniCloud"));
  QApplication::setApplicationVersion(QStringLiteral(ANICLOUD_VERSION));
  QQuickStyle::setStyle(QStringLiteral("Basic"));
  QApplication app(argc, argv);
  QNetworkInformation::loadDefaultBackend();
  const bool smokeTest = app.arguments().contains(QStringLiteral("--smoke-test"));
  const bool backgroundLaunch = app.arguments().contains(QStringLiteral("--background"));
  app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/AniCloud/resources/icon.png")));
  app.setQuitOnLastWindowClosed(false);

  const QString instanceName = QStringLiteral("ink.anicloud.desktop.instance");
  if (!smokeTest) {
    QLocalSocket existing;
    existing.connectToServer(instanceName, QIODevice::WriteOnly);
    if (existing.waitForConnected(250)) {
      const auto payload = QJsonDocument(QJsonArray::fromStringList(app.arguments().mid(1))).toJson(QJsonDocument::Compact);
      existing.write(payload);
      existing.flush();
      existing.waitForBytesWritten(1000);
      existing.disconnectFromServer();
      return 0;
    }
  }

  SecureStore secureStore;
  Database database(&secureStore);
  QString databaseError;
  if (!database.open(&databaseError)) qFatal("Unable to open native database: %s", qPrintable(databaseError));

  ProviderClient provider;
  AccountClient account(&secureStore, &database);
  HlsGateway gateway;
  PlayerController player(&provider, &account, &gateway);
  DownloadManager downloads(&database, &account, &provider);
  UpdateService updates;
  AppRuntime runtime(&database, backgroundLaunch);
  if (backgroundLaunch && (!runtime.notificationsEnabled() || !runtime.trayAvailable()))
    QTimer::singleShot(0, &app, &QCoreApplication::quit);
  DeepLinkEventFilter deepLinkFilter(&runtime);
  app.installEventFilter(&deepLinkFilter);

  QLocalServer instanceServer;
  instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
  if (!smokeTest && !instanceServer.listen(instanceName)) {
    QLocalServer::removeServer(instanceName);
    if (!instanceServer.listen(instanceName)) qWarning("Unable to start AniCloud single-instance server");
  }
  QObject::connect(&instanceServer, &QLocalServer::newConnection, &runtime, [&] {
    while (auto *socket = instanceServer.nextPendingConnection()) {
      if (!socket->waitForReadyRead(750) && socket->bytesAvailable() == 0) {
        socket->deleteLater();
        continue;
      }
      const auto document = QJsonDocument::fromJson(socket->readAll());
      const auto arguments = document.array().toVariantList();
      bool activate = true;
      for (const auto &entry : arguments) {
        const auto argument = entry.toString();
        if (argument == QStringLiteral("--background")) { activate = false; continue; }
        if (argument.startsWith(QStringLiteral("anicloud:"), Qt::CaseInsensitive) ||
            argument.startsWith(QStringLiteral("https://anicloud.ink/anime/"), Qt::CaseInsensitive)) {
          runtime.handleDeepLink(argument);
          activate = false;
        }
      }
      if (activate) runtime.requestShow();
      socket->disconnectFromServer();
      socket->deleteLater();
    }
  });

  QObject::connect(&account, &AccountClient::operationSucceeded, &runtime, [&runtime](const QString &message) { runtime.showNotification(QStringLiteral("AniCloud"), message); });
  QObject::connect(&account, &AccountClient::localProgressSaved, &runtime, &AppRuntime::refreshLocalHistory);
  QObject::connect(&account, &AccountClient::sessionExpired, &runtime, [&runtime] { runtime.setRoute(QStringLiteral("auth")); });

  QQmlApplicationEngine engine;
  auto *context = engine.rootContext();
  context->setContextProperty(QStringLiteral("Provider"), &provider);
  context->setContextProperty(QStringLiteral("Account"), &account);
  context->setContextProperty(QStringLiteral("Player"), &player);
  context->setContextProperty(QStringLiteral("Downloads"), &downloads);
  context->setContextProperty(QStringLiteral("Updates"), &updates);
  context->setContextProperty(QStringLiteral("Runtime"), &runtime);
  engine.loadFromModule(QStringLiteral("AniCloud"), QStringLiteral("Main"));
  if (engine.rootObjects().isEmpty()) return -1;

  account.restoreSession();
  account.checkMaintenance();
  account.refreshBroadcasts();
  if (!backgroundLaunch) provider.loadHome();
  QTimer broadcastTimer;
  broadcastTimer.setInterval(60 * 1000);
  QObject::connect(&broadcastTimer, &QTimer::timeout, &account, &AccountClient::refreshBroadcasts);
  broadcastTimer.start();
  if (smokeTest) QTimer::singleShot(2500, &app, &QCoreApplication::quit);
  else if (!backgroundLaunch) QTimer::singleShot(3000, &updates, &UpdateService::check);
  for (int index = 1; index < argc; ++index) {
    const auto argument = QString::fromLocal8Bit(argv[index]);
    if (!argument.startsWith(QStringLiteral("--"))) runtime.handleDeepLink(argument);
  }
  return app.exec();
}
