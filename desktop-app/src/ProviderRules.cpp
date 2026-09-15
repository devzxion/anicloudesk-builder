#include "ProviderRules.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <algorithm>

namespace {
bool fail(QString *error, const QString &message) {
  if (error) *error = message;
  return false;
}

bool exactKeys(const QJsonObject &object, const QSet<QString> &allowed) {
  for (auto it = object.begin(); it != object.end(); ++it)
    if (!allowed.contains(it.key())) return false;
  return true;
}

bool validHttpsUrl(const QString &value) {
  const QUrl url(value);
  const auto host = url.host().toLower();
  return value.size() <= 512 && url.isValid() && url.scheme() == QStringLiteral("https") &&
         url.userInfo().isEmpty() && url.fragment().isEmpty() && url.query().isEmpty() &&
         (url.port() == -1 || url.port() == 443) && host.contains(QLatin1Char('.')) &&
         !host.endsWith(QLatin1Char('.')) &&
         !QRegularExpression(QStringLiteral("^[0-9.]+$")).match(host).hasMatch() &&
         !host.endsWith(QStringLiteral(".localhost")) && !host.endsWith(QStringLiteral(".local")) &&
         !host.endsWith(QStringLiteral(".internal")) && !host.endsWith(QStringLiteral(".lan"));
}

bool validPath(QString value, const QStringList &variables) {
  if (value.isEmpty() || value.size() > 200 || value.startsWith(QLatin1Char('/')) ||
      value.contains(QStringLiteral(".."))) return false;
  for (const auto &variable : variables) {
    const auto marker = QStringLiteral("{%1}").arg(variable);
    if (!value.contains(marker)) return false;
    value.replace(marker, QStringLiteral("1"));
  }
  return QRegularExpression(QStringLiteral("^[A-Za-z0-9_/-]+$")).match(value).hasMatch();
}

QMap<QString, QString> headersFrom(const QJsonValue &value, bool *ok) {
  QMap<QString, QString> result;
  if (value.isUndefined()) return result;
  if (!value.isObject()) { *ok = false; return {}; }
  const auto object = value.toObject();
  if (object.size() > 6) { *ok = false; return {}; }
  const QSet<QString> allowed{QStringLiteral("Referer"), QStringLiteral("Origin"),
                              QStringLiteral("User-Agent"), QStringLiteral("Accept"),
                              QStringLiteral("Accept-Language"), QStringLiteral("X-Requested-With")};
  for (auto it = object.begin(); it != object.end(); ++it) {
    const auto text = it.value().toString();
    if (!allowed.contains(it.key()) || text.isEmpty() || text.size() > 512 ||
        std::any_of(text.cbegin(), text.cend(), [](QChar character) {
          return character.unicode() < 32 || character.unicode() > 126;
        }) || ((it.key() == QStringLiteral("Referer") || it.key() == QStringLiteral("Origin")) &&
               !validHttpsUrl(text))) {
      *ok = false; return {};
    }
    result.insert(it.key(), text);
  }
  return result;
}
}

QString ProviderRules::pagePath(const QString &pattern, const QString &animeId,
                                int episode, const QString &audio,
                                const QString &catalog) const {
  if (!QRegularExpression(QStringLiteral("^[0-9]+$")).match(animeId).hasMatch() ||
      episode <= 0 || (audio != QStringLiteral("sub") && audio != QStringLiteral("dub"))) return {};
  auto result = pattern;
  result.replace(QStringLiteral("{animeId}"), animeId);
  result.replace(QStringLiteral("{episode}"), QString::number(episode));
  result.replace(QStringLiteral("{audio}"), audio);
  result.replace(QStringLiteral("{catalog}"), catalog);
  return result;
}

bool ProviderRules::valid(QString *error) const {
  if (!validHttpsUrl(primaryBaseUrl) || !validHttpsUrl(secondaryBaseUrl))
    return fail(error, QStringLiteral("Provider bases must be public HTTPS origins."));
  if (!validPath(primaryPagePath, {QStringLiteral("animeId"), QStringLiteral("episode"), QStringLiteral("audio")}) ||
      !validPath(secondaryPagePath, {QStringLiteral("catalog"), QStringLiteral("animeId"), QStringLiteral("episode"), QStringLiteral("audio")}) ||
      primarySourcesPaths.isEmpty() || primarySourcesPaths.size() > 4 ||
      !validPath(secondarySourcesPath, {}))
    return fail(error, QStringLiteral("Provider paths are invalid."));
  for (const auto &path : primarySourcesPaths)
    if (!validPath(path, {})) return fail(error, QStringLiteral("A primary source path is invalid."));
  const QRegularExpression field(QStringLiteral("^[A-Za-z_][A-Za-z0-9_]{0,63}$"));
  for (const auto &value : {sourceIdParameter, sourcesField, sourceFileField, tracksField})
    if (!field.match(value).hasMatch()) return fail(error, QStringLiteral("A provider response field is invalid."));
  if (primaryCipherKey.size() < 16 || primaryCipherKey.size() > 32 ||
      primaryCipherIv.size() != 16 || primaryTokenKey.size() < 16 || primaryTokenKey.size() > 128 ||
      primaryTokenLifetimeSeconds < 30 || primaryTokenLifetimeSeconds > 600 ||
      primaryTokenRefreshLeadSeconds < 5 ||
      primaryTokenRefreshLeadSeconds >= primaryTokenLifetimeSeconds ||
      secondaryPayloadKey.size() < 8 || secondaryPayloadKey.size() > 128)
    return fail(error, QStringLiteral("Provider encryption parameters are invalid."));
  for (const auto &value : {primaryCipherKey, primaryCipherIv, primaryTokenKey,
                            secondaryPayloadKey})
    if (std::any_of(value.cbegin(), value.cend(), [](char byte) {
          const auto value = static_cast<unsigned char>(byte);
          return value < 32 || value > 126;
        })) return fail(error, QStringLiteral("Provider encryption parameters are not printable ASCII."));
  return true;
}

ProviderRules ProviderRules::fromJson(const QJsonObject &object, bool *ok, QString *error) {
  if (ok) *ok = false;
  const QSet<QString> allowed{
    QStringLiteral("primaryBaseUrl"), QStringLiteral("secondaryBaseUrl"),
    QStringLiteral("primaryPagePath"), QStringLiteral("secondaryPagePath"),
    QStringLiteral("primarySourcesPaths"), QStringLiteral("secondarySourcesPath"),
    QStringLiteral("sourceIdParameter"), QStringLiteral("sourcesField"),
    QStringLiteral("sourceFileField"), QStringLiteral("tracksField"),
    QStringLiteral("primaryCipherKey"), QStringLiteral("primaryCipherIv"),
    QStringLiteral("primaryTokenKey"), QStringLiteral("primaryMediaHeaders"),
    QStringLiteral("primaryTokenLifetimeSeconds"), QStringLiteral("primaryTokenRefreshLeadSeconds"),
    QStringLiteral("secondaryPayloadKey"),
    QStringLiteral("secondaryMediaHeaders"), QStringLiteral("relayEnabled"),
    QStringLiteral("relaySessionPath")};
  if (!exactKeys(object, allowed)) { fail(error, QStringLiteral("Provider rules contain unknown fields.")); return {}; }
  ProviderRules rules;
  const auto stringOr = [&object](const QString &key, const QString &fallback) {
    return object.contains(key) ? object.value(key).toString() : fallback;
  };
  const auto primaryBase = stringOr(QStringLiteral("primaryBaseUrl"), rules.primaryBaseUrl);
  const auto secondaryBase = stringOr(QStringLiteral("secondaryBaseUrl"), rules.secondaryBaseUrl);
  if (!primaryBase.isEmpty()) rules.primaryBaseUrl = primaryBase;
  if (!secondaryBase.isEmpty()) rules.secondaryBaseUrl = secondaryBase;
  rules.primaryPagePath = stringOr(QStringLiteral("primaryPagePath"), rules.primaryPagePath);
  rules.secondaryPagePath = stringOr(QStringLiteral("secondaryPagePath"), rules.secondaryPagePath);
  if (object.contains(QStringLiteral("primarySourcesPaths"))) {
    rules.primarySourcesPaths.clear();
    for (const auto &item : object.value(QStringLiteral("primarySourcesPaths")).toArray())
      rules.primarySourcesPaths.append(item.toString());
  }
  rules.secondarySourcesPath = stringOr(QStringLiteral("secondarySourcesPath"), rules.secondarySourcesPath);
  rules.sourceIdParameter = stringOr(QStringLiteral("sourceIdParameter"), rules.sourceIdParameter);
  rules.sourcesField = stringOr(QStringLiteral("sourcesField"), rules.sourcesField);
  rules.sourceFileField = stringOr(QStringLiteral("sourceFileField"), rules.sourceFileField);
  rules.tracksField = stringOr(QStringLiteral("tracksField"), rules.tracksField);
  rules.primaryCipherKey = stringOr(QStringLiteral("primaryCipherKey"), QString::fromUtf8(rules.primaryCipherKey)).toUtf8();
  rules.primaryCipherIv = stringOr(QStringLiteral("primaryCipherIv"), QString::fromUtf8(rules.primaryCipherIv)).toUtf8();
  rules.primaryTokenKey = stringOr(QStringLiteral("primaryTokenKey"), QString::fromUtf8(rules.primaryTokenKey)).toUtf8();
  if (object.contains(QStringLiteral("primaryTokenLifetimeSeconds")))
    rules.primaryTokenLifetimeSeconds = object.value(QStringLiteral("primaryTokenLifetimeSeconds")).toInt();
  if (object.contains(QStringLiteral("primaryTokenRefreshLeadSeconds")))
    rules.primaryTokenRefreshLeadSeconds = object.value(QStringLiteral("primaryTokenRefreshLeadSeconds")).toInt();
  rules.secondaryPayloadKey = stringOr(QStringLiteral("secondaryPayloadKey"),
                                       QString::fromUtf8(rules.secondaryPayloadKey)).toUtf8();
  bool headersOk = true;
  rules.primaryMediaHeaders = headersFrom(object.value(QStringLiteral("primaryMediaHeaders")), &headersOk);
  rules.secondaryMediaHeaders = headersFrom(object.value(QStringLiteral("secondaryMediaHeaders")), &headersOk);
  if (object.contains(QStringLiteral("relayEnabled")) &&
      !object.value(QStringLiteral("relayEnabled")).isBool()) headersOk = false;
  if (object.contains(QStringLiteral("relaySessionPath")) &&
      !validPath(object.value(QStringLiteral("relaySessionPath")).toString(), {})) headersOk = false;
  if (!headersOk || !rules.valid(error)) return {};
  if (ok) *ok = true;
  return rules;
}

QStringList providerFallbackOrder(const QString &requested) {
  const auto server = requested.trimmed().toLower();
  if (server == QStringLiteral("hd-1"))
    return {QStringLiteral("hd-1"), QStringLiteral("hd-3"), QStringLiteral("hd-4")};
  if (server == QStringLiteral("hd-2"))
    return {QStringLiteral("hd-2"), QStringLiteral("hd-4"), QStringLiteral("hd-3")};
  if (server == QStringLiteral("hd-3"))
    return {QStringLiteral("hd-3"), QStringLiteral("hd-4"), QStringLiteral("hd-1")};
  return {QStringLiteral("hd-4"), QStringLiteral("hd-3"), QStringLiteral("hd-1")};
}

QList<QPair<QString, QString>> zokoRouteCandidates(const QString &server,
                                                   const QString &malId,
                                                   const QString &anilistId) {
  QList<QPair<QString, QString>> result;
  if (server == QStringLiteral("hd-4") && !malId.isEmpty())
    result.append({QStringLiteral("mal"), malId});
  if (!anilistId.isEmpty()) result.append({QStringLiteral("ani"), anilistId});
  if (server == QStringLiteral("hd-3") && !malId.isEmpty())
    result.append({QStringLiteral("mal"), malId});
  return result;
}
