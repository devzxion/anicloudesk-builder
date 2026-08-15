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
  app.setWindowIcon(QIcon(QStringLiteral(":/qt/qml/AniCloud/resources/icon.png")));

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
  AppRuntime runtime(&database);
  DeepLinkEventFilter deepLinkFilter(&runtime);
  app.installEventFilter(&deepLinkFilter);

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
  provider.loadHome();
  if (smokeTest) QTimer::singleShot(2500, &app, &QCoreApplication::quit);
  else QTimer::singleShot(3000, &updates, &UpdateService::check);
  for (int index = 1; index < argc; ++index) runtime.handleDeepLink(QString::fromLocal8Bit(argv[index]));
  return app.exec();
}
