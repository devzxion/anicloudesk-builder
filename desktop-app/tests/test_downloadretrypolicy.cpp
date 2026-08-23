#include "DownloadRetryPolicy.h"

#include <QDateTime>
#include <QTest>

class DownloadRetryPolicyTest final : public QObject {
  Q_OBJECT
private slots:
  void retriesOnlyTransientFailures() {
    QVERIFY(DownloadRetryPolicy::shouldRetry(429, QNetworkReply::NoError));
    QVERIFY(DownloadRetryPolicy::shouldRetry(503, QNetworkReply::NoError));
    QVERIFY(DownloadRetryPolicy::shouldRetry(0, QNetworkReply::TimeoutError));
    QVERIFY(DownloadRetryPolicy::shouldRetry(0, QNetworkReply::RemoteHostClosedError));
    QVERIFY(!DownloadRetryPolicy::shouldRetry(401, QNetworkReply::AuthenticationRequiredError));
    QVERIFY(!DownloadRetryPolicy::shouldRetry(404, QNetworkReply::ContentNotFoundError));
    QVERIFY(!DownloadRetryPolicy::shouldRetry(0, QNetworkReply::OperationCanceledError));
  }

  void appliesBoundedExponentialBackoff() {
    QCOMPARE(DownloadRetryPolicy::delayMs(1), 1500);
    QCOMPARE(DownloadRetryPolicy::delayMs(2), 3000);
    QCOMPARE(DownloadRetryPolicy::delayMs(3), 6000);
    QCOMPARE(DownloadRetryPolicy::delayMs(8), 60000);
    QCOMPARE(DownloadRetryPolicy::delayMs(99), 60000);
  }

  void honorsRetryAfterSecondsAndDates() {
    QCOMPARE(DownloadRetryPolicy::delayMs(1, QByteArrayLiteral("12")), 12000);
    QCOMPARE(DownloadRetryPolicy::delayMs(1, QByteArrayLiteral("9999")), 120000);
    const auto now = QDateTime::fromString(QStringLiteral("Sun, 23 Aug 2026 10:00:00 +0000"), Qt::RFC2822Date);
    QCOMPARE(DownloadRetryPolicy::delayMs(1, QByteArrayLiteral("Sun, 23 Aug 2026 10:00:15 +0000"), now.toMSecsSinceEpoch()), 15000);
  }
};

QTEST_GUILESS_MAIN(DownloadRetryPolicyTest)
#include "test_downloadretrypolicy.moc"
