#include "DownloadRetryPolicy.h"

#include <QDateTime>
#include <QtGlobal>

namespace DownloadRetryPolicy {

bool shouldRetry(int httpStatus, QNetworkReply::NetworkError error) {
  switch (httpStatus) {
    case 408:
    case 425:
    case 429:
    case 500:
    case 502:
    case 503:
    case 504:
      return true;
    default:
      break;
  }
  if (httpStatus >= 400) return false;
  switch (error) {
    case QNetworkReply::ConnectionRefusedError:
    case QNetworkReply::RemoteHostClosedError:
    case QNetworkReply::HostNotFoundError:
    case QNetworkReply::TimeoutError:
    case QNetworkReply::TemporaryNetworkFailureError:
    case QNetworkReply::NetworkSessionFailedError:
    case QNetworkReply::ProxyConnectionRefusedError:
    case QNetworkReply::ProxyConnectionClosedError:
    case QNetworkReply::ProxyNotFoundError:
    case QNetworkReply::ProxyTimeoutError:
    case QNetworkReply::ContentReSendError:
    case QNetworkReply::UnknownNetworkError:
    case QNetworkReply::UnknownProxyError:
      return true;
    default:
      return false;
  }
}

int delayMs(int attempt, const QByteArray &retryAfter, qint64 nowMs) {
  const auto normalizedAttempt = qBound(1, attempt, MaxAutomaticRetries);
  qint64 delay = BaseDelayMs;
  for (int index = 1; index < normalizedAttempt && delay < MaximumDelayMs; ++index)
    delay = qMin<qint64>(MaximumDelayMs, delay * 2);

  bool secondsValid = false;
  const auto seconds = retryAfter.trimmed().toLongLong(&secondsValid);
  if (secondsValid && seconds >= 0) {
    delay = qMax(delay, qMin<qint64>(MaximumRetryAfterMs, seconds * 1000));
  } else if (!retryAfter.trimmed().isEmpty()) {
    const auto retryAt = QDateTime::fromString(QString::fromLatin1(retryAfter.trimmed()), Qt::RFC2822Date);
    if (retryAt.isValid()) {
      const auto current = nowMs >= 0 ? nowMs : QDateTime::currentMSecsSinceEpoch();
      delay = qMax(delay, qMin<qint64>(MaximumRetryAfterMs, retryAt.toMSecsSinceEpoch() - current));
    }
  }
  return static_cast<int>(qBound<qint64>(static_cast<qint64>(BaseDelayMs), delay,
                                         static_cast<qint64>(MaximumRetryAfterMs)));
}

}
