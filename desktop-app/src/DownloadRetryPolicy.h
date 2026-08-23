#pragma once

#include <QByteArray>
#include <QNetworkReply>

namespace DownloadRetryPolicy {

inline constexpr int MaxAutomaticRetries = 8;
inline constexpr int BaseDelayMs = 1500;
inline constexpr int MaximumDelayMs = 60'000;
inline constexpr int MaximumRetryAfterMs = 120'000;

[[nodiscard]] bool shouldRetry(int httpStatus, QNetworkReply::NetworkError error);
[[nodiscard]] int delayMs(int attempt, const QByteArray &retryAfter = {}, qint64 nowMs = -1);

}
