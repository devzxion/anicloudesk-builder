#pragma once

#include <QByteArray>
#include <QString>
#include <QJsonObject>

namespace ProviderCrypto {

QString decodeEncryptedFile(const QString &encoded, const QByteArray &key,
                            const QByteArray &iv);
QString signMediaUrl(const QString &url, const QByteArray &tokenKey,
                     qint64 nowSeconds, int lifetimeSeconds = 90);
QString refreshSignedUrl(const QString &url, const QByteArray &tokenKey,
                         qint64 nowSeconds, int lifetimeSeconds = 90,
                         int refreshLeadSeconds = 30);
QJsonObject decodeXorPayload(const QString &encoded, const QByteArray &key);

} // namespace ProviderCrypto
