#include "ApiClient.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTextStream>
#include <QTimer>

class ProviderLiveSmoke final : public QObject {
  Q_OBJECT

public:
  explicit ProviderLiveSmoke(QObject *parent = nullptr) : QObject(parent) {
    for (const auto &server : {QStringLiteral("hd-1"), QStringLiteral("hd-2"),
                               QStringLiteral("hd-3"), QStringLiteral("hd-4")}) {
      m_cases.append({server, QStringLiteral("sub")});
      m_cases.append({server, QStringLiteral("dub")});
    }
    connect(&m_provider, &ProviderClient::streamResolved, this,
            &ProviderLiveSmoke::resolved);
    connect(&m_provider, &ProviderClient::streamFailed, this,
            [this](int generation, const QString &) {
      if (generation != m_generation) return;
      finishCurrent(generation, false);
    });
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(45'000);
    connect(&m_timeout, &QTimer::timeout, this, [this] { finishCurrent(m_generation, false); });
  }

  void start() { next(); }

private:
  void next() {
    if (++m_index >= m_cases.size()) {
      QTextStream(stdout) << "Provider smoke: " << (m_failures == 0 ? "PASS" : "FAIL")
                          << " (" << (m_cases.size() - m_failures) << "/" << m_cases.size()
                          << ")\n";
      QCoreApplication::exit(m_failures == 0 ? 0 : 1);
      return;
    }
    ++m_generation;
    m_timeout.start();
    const auto &test = m_cases.at(m_index);
    m_provider.resolveStream(m_generation, QStringLiteral("20::ep=1"),
                             test.first, test.second);
  }

  void resolved(int generation, const QVariantMap &stream) {
    if (generation != m_generation) return;
    QNetworkRequest request(QUrl(stream.value(QStringLiteral("mediaUrl")).toString()));
    request.setTransferTimeout(25'000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    const auto headers = stream.value(QStringLiteral("headers")).toMap();
    for (auto it = headers.cbegin(); it != headers.cend(); ++it)
      request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
    request.setRawHeader(QByteArrayLiteral("Range"), QByteArrayLiteral("bytes=0-8191"));
    auto *reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
      const auto body = reply->read(8192).trimmed();
      const bool passed = reply->error() == QNetworkReply::NoError &&
                          body.startsWith(QByteArrayLiteral("#EXTM3U"));
      reply->deleteLater();
      finishCurrent(generation, passed);
    });
  }

  void finishCurrent(int generation, bool passed) {
    if (generation != m_generation || !m_timeout.isActive()) return;
    m_timeout.stop();
    const auto &test = m_cases.at(m_index);
    QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << test.first << ' '
                        << test.second << '\n';
    if (!passed) ++m_failures;
    QTimer::singleShot(0, this, &ProviderLiveSmoke::next);
  }

  ProviderClient m_provider;
  QNetworkAccessManager m_network;
  QTimer m_timeout;
  QList<QPair<QString, QString>> m_cases;
  int m_index = -1;
  int m_generation = 0;
  int m_failures = 0;
};

int main(int argc, char **argv) {
  QCoreApplication application(argc, argv);
  ProviderLiveSmoke smoke;
  QTimer::singleShot(0, &smoke, &ProviderLiveSmoke::start);
  return application.exec();
}

#include "provider_live_smoke.moc"
