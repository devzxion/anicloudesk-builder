#pragma once

#include "ProviderRules.h"

#include <QNetworkAccessManager>
#include <QObject>

class ProviderPatchManager final : public QObject {
  Q_OBJECT

public:
  explicit ProviderPatchManager(QObject *parent = nullptr);
  [[nodiscard]] ProviderRules activeRules() const;
  [[nodiscard]] qint64 installedRevision() const { return m_installed.revision; }
  Q_INVOKABLE void refresh();

  static bool verifyEnvelope(const QByteArray &envelope, const QByteArray &publicKeyBase64,
                             int versionCode, qint64 nowSeconds, ProviderPatch *patch,
                             bool allowExpired = false);

signals:
  void rulesChanged();

private:
  void load();
  bool install(const QByteArray &envelope, bool persist);

  QNetworkAccessManager m_network;
  ProviderPatch m_installed;
  QString m_path;
  qint64 m_lastAttemptMs = 0;
};

