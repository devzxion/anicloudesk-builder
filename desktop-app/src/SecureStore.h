#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class SecureStore final : public QObject {
  Q_OBJECT

public:
  explicit SecureStore(QObject *parent = nullptr, const QByteArray &initialMasterKey = {});

  [[nodiscard]] QString readText(const QString &key, QString *error = nullptr) const;
  bool writeText(const QString &key, const QString &value, QString *error = nullptr) const;
  bool remove(const QString &key, QString *error = nullptr) const;

  [[nodiscard]] QByteArray seal(const QByteArray &plainText, QString *error = nullptr);
  [[nodiscard]] QByteArray open(const QByteArray &cipherText, QString *error = nullptr);
  [[nodiscard]] bool available() const { return m_sodiumReady; }

private:
  [[nodiscard]] QByteArray masterKey(QString *error);

  QString m_service;
  bool m_sodiumReady = false;
  QByteArray m_cachedMasterKey;
};
