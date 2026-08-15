#include "PlayerController.h"

#include "ApiClient.h"
#include "HlsGateway.h"

#include <QFileInfo>
#include <QMediaMetaData>
#include <QSettings>
#include <QUrl>

PlayerController::PlayerController(ProviderClient *provider, AccountClient *account, HlsGateway *gateway, QObject *parent)
  : QObject(parent), m_provider(provider), m_account(account), m_gateway(gateway) {
  m_player.setAudioOutput(&m_audio);
  m_volume = qBound(0.0, QSettings().value(QStringLiteral("playback/volume"), 0.8).toDouble(), 1.0);
  m_volumeBoost = qBound(1.0, QSettings().value(QStringLiteral("playback/volumeBoost"), 1.0).toDouble(), 2.0);
  applyAudioGain();
  m_quality = QSettings().value(QStringLiteral("playback/quality"), QStringLiteral("auto")).toString().toLower();
  connect(&m_player, &QMediaPlayer::positionChanged, this, [this](qint64 position) {
    if (position > m_lastProgressPosition + 500) {
      m_lastProgressPosition = position;
      m_bufferRetries = 0;
    }
    emit positionChanged();
  });
  connect(&m_player, &QMediaPlayer::durationChanged, this, &PlayerController::durationChanged);
  connect(&m_player, &QMediaPlayer::bufferProgressChanged, this, &PlayerController::bufferProgressChanged);
  connect(&m_player, &QMediaPlayer::playbackRateChanged, this, &PlayerController::speedChanged);
  connect(&m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
    if (state == QMediaPlayer::PlayingState) { m_bufferTimer.stop(); setState(QStringLiteral("playing")); }
    else if (state == QMediaPlayer::PausedState) { setState(QStringLiteral("paused")); saveProgress(); }
    else if (!m_player.source().isEmpty() && m_player.mediaStatus() == QMediaPlayer::EndOfMedia) { saveProgress(); nextEpisode(); }
  });
  connect(&m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
    if (status == QMediaPlayer::LoadingMedia || status == QMediaPlayer::BufferingMedia || status == QMediaPlayer::StalledMedia) {
      setState(QStringLiteral("buffering"));
      if (!m_bufferTimer.isActive()) m_bufferTimer.start();
    } else {
      m_bufferTimer.stop();
    }
    if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia) {
      if (m_restorePosition > 0) {
        m_player.setPosition(m_player.duration() > 0 ? qMin(m_restorePosition, m_player.duration()) : m_restorePosition);
        m_restorePosition = 0;
      }
      m_player.setPlaybackRate(m_restoreSpeed);
      refreshTracks();
      if (m_restorePlaying) m_player.play(); else m_player.pause();
    } else if (status == QMediaPlayer::InvalidMedia) {
      failOrFallback(m_player.errorString());
    }
  });
  connect(&m_player, &QMediaPlayer::errorOccurred, this, [this](QMediaPlayer::Error, const QString &message) { failOrFallback(message); });
  connect(&m_player, &QMediaPlayer::tracksChanged, this, &PlayerController::refreshTracks);
  connect(m_provider, &ProviderClient::streamResolved, this, &PlayerController::applyStream);
  connect(m_provider, &ProviderClient::streamFailed, this, [this](int generation, const QString &message) {
    if (generation == m_generation) failOrFallback(message);
  });

  m_progressTimer.setInterval(15'000);
  connect(&m_progressTimer, &QTimer::timeout, this, &PlayerController::saveProgress);
  m_progressTimer.start();
  m_bufferTimer.setSingleShot(true);
  m_bufferTimer.setInterval(15'000);
  connect(&m_bufferTimer, &QTimer::timeout, this, [this] {
    if (++m_bufferRetries <= 1) { const auto at = position(); m_player.stop(); m_restorePosition = at; loadStream(m_stream); }
    else failOrFallback(QStringLiteral("Playback remained stalled."));
  });
}

void PlayerController::attachVideoSink(QObject *sink) {
  m_player.setVideoSink(qobject_cast<QVideoSink *>(sink));
}

void PlayerController::setState(const QString &state) {
  if (m_state == state) return; m_state = state; emit stateChanged();
}

void PlayerController::setError(const QString &error) {
  if (m_error == error) return; m_error = error; emit errorChanged();
}

void PlayerController::open(const QVariantMap &episode, qint64 resumeMilliseconds) {
  const auto configuredQuality = QSettings().value(QStringLiteral("playback/quality"), QStringLiteral("auto")).toString().toLower();
  if (configuredQuality != m_quality) { m_quality = configuredQuality; emit qualityChanged(); }
  m_restoreSpeed = speed();
  m_restoreCaptionIndex = m_player.activeSubtitleTrack();
  saveProgress();
  m_current = episode;
  if (!m_current.contains(QStringLiteral("episodeNumber"))) m_current.insert(QStringLiteral("episodeNumber"), episode.value(QStringLiteral("number")));
  if (!m_current.contains(QStringLiteral("episodeName"))) m_current.insert(QStringLiteral("episodeName"), episode.value(QStringLiteral("title"), QStringLiteral("Episode %1").arg(episode.value(QStringLiteral("number")).toInt())));
  m_server = episode.value(QStringLiteral("server"), QStringLiteral("hd-1")).toString();
  m_audioMode = episode.value(QStringLiteral("audioMode"), QStringLiteral("sub")).toString();
  m_restorePosition = resumeMilliseconds;
  m_restorePlaying = true;
  m_triedSecondaryServer = false;
  m_alternateIndex = -1;
  emit currentChanged();
  m_provider->loadServers(m_current.value(QStringLiteral("episodeId"), m_current.value(QStringLiteral("id"))).toString());
  resolve(false);
}

void PlayerController::openOffline(const QVariantMap &download) {
  if (download.value(QStringLiteral("state")).toString() != QStringLiteral("completed")) {
    setError(QStringLiteral("This offline episode is incomplete.")); setState(QStringLiteral("error")); return;
  }
  const auto path = download.value(QStringLiteral("rootPath")).toString() + QStringLiteral("/offline.m3u8");
  if (!QFileInfo::exists(path)) { setError(QStringLiteral("The offline library is missing its playlist.")); setState(QStringLiteral("error")); return; }
  if (!m_sessionId.isEmpty()) m_gateway->closeSession(m_sessionId);
  m_sessionId.clear();
  m_current = download; m_stream = download;
  m_server = download.value(QStringLiteral("server"), QStringLiteral("offline")).toString();
  m_audioMode = download.value(QStringLiteral("audioMode"), QStringLiteral("sub")).toString();
  m_captions = download.value(QStringLiteral("subtitles")).toList();
  m_restorePosition = download.value(QStringLiteral("positionMilliseconds")).toLongLong();
  m_restorePlaying = true; m_bufferRetries = 0; m_lastProgressPosition = 0;
  emit captionsChanged(); emit currentChanged(); setError({}); setState(QStringLiteral("loading"));
  m_player.setSource(QUrl::fromLocalFile(path));
  m_player.play();
}

void PlayerController::resolve(bool preserveState) {
  if (preserveState) {
    m_restorePosition = position(); m_restorePlaying = playing(); m_restoreSpeed = speed();
    m_restoreCaptionIndex = m_player.activeSubtitleTrack();
  }
  setError({}); setState(QStringLiteral("resolving")); ++m_generation; m_alternateIndex = -1; m_bufferRetries = 0;
  m_lastProgressPosition = 0;
  m_provider->resolveStream(m_generation, m_current.value(QStringLiteral("episodeId"), m_current.value(QStringLiteral("id"))).toString(), m_server, m_audioMode);
}

void PlayerController::applyStream(int generation, const QVariantMap &stream) {
  if (generation != m_generation) return;
  m_stream = stream;
  if (m_quality != QStringLiteral("auto")) {
    for (const auto &alternateValue : stream.value(QStringLiteral("alternates")).toList()) {
      const auto alternate = alternateValue.toMap();
      if (alternate.value(QStringLiteral("label")).toString().toLower().contains(m_quality)) {
        m_stream.insert(QStringLiteral("mediaUrl"), alternate.value(QStringLiteral("url"), alternate.value(QStringLiteral("file"))));
        break;
      }
    }
  }
  m_captions = m_stream.value(QStringLiteral("subtitles")).toList(); emit captionsChanged(); emit currentChanged();
  loadStream(m_stream);
}

void PlayerController::loadStream(const QVariantMap &stream) {
  if (!m_sessionId.isEmpty()) m_gateway->closeSession(m_sessionId);
  const auto local = m_gateway->openSession(stream);
  if (local.isEmpty()) { failOrFallback(QStringLiteral("The native playback gateway could not open this source.")); return; }
  const auto parts = QUrl(local).path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  m_sessionId = parts.size() >= 2 ? parts.at(1) : QString{};
  setError({}); setState(QStringLiteral("loading"));
  m_player.setSource(QUrl(local));
  m_player.play();
}

void PlayerController::failOrFallback(const QString &message) {
  const auto alternates = m_stream.value(QStringLiteral("alternates")).toList();
  while (++m_alternateIndex < alternates.size()) {
    auto alternate = m_stream;
    const auto item = alternates.at(m_alternateIndex).toMap();
    const auto url = item.value(QStringLiteral("url"), item.value(QStringLiteral("file"))).toString();
    if (!url.isEmpty() && url != m_stream.value(QStringLiteral("mediaUrl")).toString()) {
      if (m_restorePosition <= 0) m_restorePosition = position();
      m_restoreSpeed = speed(); m_restoreCaptionIndex = m_player.activeSubtitleTrack();
      alternate.insert(QStringLiteral("mediaUrl"), url); m_stream = alternate; loadStream(m_stream); return;
    }
  }
  if (!m_triedSecondaryServer) {
    m_triedSecondaryServer = true;
    m_server = m_server == QStringLiteral("hd-2") ? QStringLiteral("hd-1") : QStringLiteral("hd-2");
    if (m_restorePosition <= 0) m_restorePosition = position();
    m_restoreSpeed = speed(); m_restoreCaptionIndex = m_player.activeSubtitleTrack();
    resolve(false); return;
  }
  setError(message.isEmpty() ? QStringLiteral("This episode could not be played.") : message);
  setState(QStringLiteral("error"));
}

void PlayerController::play() { m_restorePlaying = true; m_player.play(); }
void PlayerController::pause() { m_restorePlaying = false; m_player.pause(); }
void PlayerController::togglePlayback() { playing() ? pause() : play(); }
void PlayerController::seek(qint64 milliseconds) { m_player.setPosition(qBound<qint64>(0, milliseconds, duration())); }
void PlayerController::seekBy(qint64 deltaMilliseconds) { seek(position() + deltaMilliseconds); }
void PlayerController::setVolume(double volume) {
  const auto value = qBound(0.0, volume, 1.0);
  if (qFuzzyCompare(m_volume, value)) return;
  m_volume = value; QSettings().setValue(QStringLiteral("playback/volume"), value);
  applyAudioGain(); emit volumeChanged();
}
void PlayerController::adjustVolume(double delta) { setVolume(volume() + delta); }
void PlayerController::setVolumeBoost(double boost) {
  const auto value = qBound(1.0, boost, 2.0);
  if (qFuzzyCompare(m_volumeBoost, value)) return;
  m_volumeBoost = value; QSettings().setValue(QStringLiteral("playback/volumeBoost"), value);
  applyAudioGain(); emit volumeBoostChanged();
}
void PlayerController::cycleVolumeBoost() {
  if (m_volumeBoost < 1.24) setVolumeBoost(1.25);
  else if (m_volumeBoost < 1.49) setVolumeBoost(1.5);
  else if (m_volumeBoost < 1.99) setVolumeBoost(2.0);
  else setVolumeBoost(1.0);
}
void PlayerController::applyAudioGain() { m_audio.setVolume(static_cast<float>(qMin(1.0, m_volume * m_volumeBoost))); }
void PlayerController::setSpeed(double speed) { m_player.setPlaybackRate(qBound(0.25, speed, 3.0)); }
void PlayerController::setQuality(const QString &quality) {
  const auto normalized = quality.trimmed().toLower();
  if (normalized.isEmpty() || normalized == m_quality) return;
  m_quality = normalized; QSettings().setValue(QStringLiteral("playback/quality"), normalized); emit qualityChanged();
  if (!m_current.isEmpty()) resolve(true);
}
void PlayerController::setMuted(bool muted) { if (m_audio.isMuted() == muted) return; m_audio.setMuted(muted); emit mutedChanged(); }
void PlayerController::toggleMuted() { setMuted(!muted()); }

void PlayerController::setCaptionsEnabled(bool enabled) {
  if (m_captionsEnabled == enabled) return; m_captionsEnabled = enabled;
  m_player.setActiveSubtitleTrack(enabled && !m_player.subtitleTracks().isEmpty() ? 0 : -1); emit captionsChanged();
}

void PlayerController::selectCaption(int index) {
  if (index >= 0 && index < m_player.subtitleTracks().size()) { m_captionsEnabled = true; m_player.setActiveSubtitleTrack(index); }
  else { m_captionsEnabled = false; m_player.setActiveSubtitleTrack(-1); }
  emit captionsChanged();
}

void PlayerController::refreshTracks() {
  if (m_player.subtitleTracks().isEmpty()) return;
  const auto preferred = m_restoreCaptionIndex >= 0 ? qMin(m_restoreCaptionIndex, m_player.subtitleTracks().size() - 1) : qMax(0, m_player.activeSubtitleTrack());
  m_player.setActiveSubtitleTrack(m_captionsEnabled ? preferred : -1);
  m_restoreCaptionIndex = -1;
}

void PlayerController::switchServer(const QString &server) { if (server == m_server) return; m_server = server; m_current.insert(QStringLiteral("server"), server); emit currentChanged(); m_triedSecondaryServer = false; resolve(true); }
void PlayerController::switchAudio(const QString &audioMode) { if (audioMode == m_audioMode) return; m_audioMode = audioMode; m_current.insert(QStringLiteral("audioMode"), audioMode); emit currentChanged(); m_triedSecondaryServer = false; resolve(true); }
void PlayerController::retry() { m_triedSecondaryServer = false; resolve(m_state != QStringLiteral("error")); }
void PlayerController::skipIntro() { if (introEnd() > 0) seek(introEnd()); }
void PlayerController::skipOutro() { if (outroEnd() > 0) seek(outroEnd()); else nextEpisode(); }

void PlayerController::saveProgress() {
  if (m_current.isEmpty() || position() <= 0) return;
  auto value = m_current;
  value.insert(QStringLiteral("episodeId"), m_current.value(QStringLiteral("episodeId"), m_current.value(QStringLiteral("id"))));
  value.insert(QStringLiteral("animeId"), m_current.value(QStringLiteral("animeId")));
  value.insert(QStringLiteral("positionSeconds"), position() / 1000);
  value.insert(QStringLiteral("durationSeconds"), duration() / 1000);
  value.insert(QStringLiteral("server"), m_server);
  value.insert(QStringLiteral("audioMode"), m_audioMode);
  m_account->saveProgress(value);
}

void PlayerController::close() {
  saveProgress(); m_player.stop(); m_player.setSource(QUrl{});
  if (!m_sessionId.isEmpty()) m_gateway->closeSession(m_sessionId);
  m_sessionId.clear(); m_current.clear(); m_stream.clear(); m_captions.clear();
  setError({}); setState(QStringLiteral("idle")); emit captionsChanged(); emit currentChanged();
}

void PlayerController::nextEpisode() { saveProgress(); emit requestNextEpisode(m_current); }
