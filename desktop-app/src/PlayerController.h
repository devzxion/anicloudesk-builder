#pragma once

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QVideoSink>

class AccountClient;
class HlsGateway;
class ProviderClient;

class PlayerController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString state READ state NOTIFY stateChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
  Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
  Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
  Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)
  Q_PROPERTY(QString quality READ quality WRITE setQuality NOTIFY qualityChanged)
  Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
  Q_PROPERTY(bool captionsEnabled READ captionsEnabled WRITE setCaptionsEnabled NOTIFY captionsChanged)
  Q_PROPERTY(QVariantList captions READ captions NOTIFY captionsChanged)
  Q_PROPERTY(QVariantMap current READ current NOTIFY currentChanged)
  Q_PROPERTY(qint64 introStart READ introStart NOTIFY currentChanged)
  Q_PROPERTY(qint64 introEnd READ introEnd NOTIFY currentChanged)
  Q_PROPERTY(qint64 outroStart READ outroStart NOTIFY currentChanged)
  Q_PROPERTY(qint64 outroEnd READ outroEnd NOTIFY currentChanged)

public:
  PlayerController(ProviderClient *provider, AccountClient *account, HlsGateway *gateway, QObject *parent = nullptr);

  [[nodiscard]] QString state() const { return m_state; }
  [[nodiscard]] QString error() const { return m_error; }
  [[nodiscard]] qint64 position() const { return m_player.position(); }
  [[nodiscard]] qint64 duration() const { return m_player.duration(); }
  [[nodiscard]] double volume() const { return m_audio.volume(); }
  [[nodiscard]] double speed() const { return m_player.playbackRate(); }
  [[nodiscard]] QString quality() const { return m_quality; }
  [[nodiscard]] bool muted() const { return m_audio.isMuted(); }
  [[nodiscard]] bool playing() const { return m_player.playbackState() == QMediaPlayer::PlayingState; }
  [[nodiscard]] bool captionsEnabled() const { return m_captionsEnabled; }
  [[nodiscard]] QVariantList captions() const { return m_captions; }
  [[nodiscard]] QVariantMap current() const { return m_current; }
  [[nodiscard]] qint64 introStart() const { return m_stream.value(QStringLiteral("introStart")).toLongLong() * 1000; }
  [[nodiscard]] qint64 introEnd() const { return m_stream.value(QStringLiteral("introEnd")).toLongLong() * 1000; }
  [[nodiscard]] qint64 outroStart() const { return m_stream.value(QStringLiteral("outroStart")).toLongLong() * 1000; }
  [[nodiscard]] qint64 outroEnd() const { return m_stream.value(QStringLiteral("outroEnd")).toLongLong() * 1000; }

  Q_INVOKABLE void open(const QVariantMap &episode, qint64 resumeMilliseconds = 0);
  Q_INVOKABLE void attachVideoSink(QObject *sink);
  Q_INVOKABLE void openOffline(const QVariantMap &download);
  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void togglePlayback();
  Q_INVOKABLE void seek(qint64 milliseconds);
  Q_INVOKABLE void seekBy(qint64 deltaMilliseconds);
  Q_INVOKABLE void setVolume(double volume);
  Q_INVOKABLE void adjustVolume(double delta);
  Q_INVOKABLE void setSpeed(double speed);
  Q_INVOKABLE void setQuality(const QString &quality);
  Q_INVOKABLE void setMuted(bool muted);
  Q_INVOKABLE void toggleMuted();
  Q_INVOKABLE void setCaptionsEnabled(bool enabled);
  Q_INVOKABLE void selectCaption(int index);
  Q_INVOKABLE void switchServer(const QString &server);
  Q_INVOKABLE void switchAudio(const QString &audioMode);
  Q_INVOKABLE void retry();
  Q_INVOKABLE void skipIntro();
  Q_INVOKABLE void skipOutro();
  Q_INVOKABLE void close();
  Q_INVOKABLE void nextEpisode();

signals:
  void stateChanged();
  void errorChanged();
  void positionChanged();
  void durationChanged();
  void volumeChanged();
  void speedChanged();
  void qualityChanged();
  void mutedChanged();
  void captionsChanged();
  void currentChanged();
  void requestNextEpisode(const QVariantMap &current);

private:
  void resolve(bool preserveState);
  void applyStream(int generation, const QVariantMap &stream);
  void loadStream(const QVariantMap &stream);
  void failOrFallback(const QString &message);
  void saveProgress();
  void setState(const QString &state);
  void setError(const QString &error);
  void refreshTracks();

  ProviderClient *m_provider = nullptr;
  AccountClient *m_account = nullptr;
  HlsGateway *m_gateway = nullptr;
  QMediaPlayer m_player;
  QAudioOutput m_audio;
  QTimer m_progressTimer;
  QTimer m_bufferTimer;
  QVariantMap m_current;
  QVariantMap m_stream;
  QVariantList m_captions;
  QString m_state = QStringLiteral("idle");
  QString m_error;
  QString m_server = QStringLiteral("hd-1");
  QString m_audioMode = QStringLiteral("sub");
  QString m_quality = QStringLiteral("auto");
  QString m_sessionId;
  qint64 m_restorePosition = 0;
  bool m_restorePlaying = true;
  double m_restoreSpeed = 1.0;
  int m_restoreCaptionIndex = -1;
  bool m_captionsEnabled = true;
  int m_generation = 0;
  int m_alternateIndex = -1;
  int m_bufferRetries = 0;
  bool m_triedSecondaryServer = false;
};
