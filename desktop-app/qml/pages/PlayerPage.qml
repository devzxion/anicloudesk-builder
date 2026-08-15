import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import AniCloud

Rectangle {
    id: root
    color: "black"
    focus: true
    property bool controlsVisible: true

    VideoOutput { id: videoOutput; anchors.fill: parent; fillMode: VideoOutput.PreserveAspectFit; Component.onCompleted: Player.attachVideoSink(videoSink) }
    MouseArea { anchors.fill: parent; onClicked: { Player.togglePlayback(); root.controlsVisible = true; hideTimer.restart() } onPositionChanged: { root.controlsVisible = true; hideTimer.restart() } }
    Timer { id: hideTimer; interval: 3000; onTriggered: if (Player.playing) root.controlsVisible = false }

    Rectangle {
        anchors.fill: parent; visible: root.controlsVisible; color: "#22000000"
        gradient: Gradient { GradientStop { position: 0; color: "#66000000" } GradientStop { position: 0.45; color: "#00000000" } GradientStop { position: 1; color: "#CC000000" } }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 24; spacing: 10
            Slider { Layout.fillWidth: true; from: 0; to: Math.max(1, Player.duration); value: Player.position; onMoved: Player.seek(value); Accessible.name: "Playback position"; palette.highlight: Theme.red }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: Player.playing ? "Pause" : "Play"; compact: true; onClicked: Player.togglePlayback() }
                AppButton { text: "−10s"; compact: true; secondary: true; onClicked: Player.seekBy(-10000) }
                AppButton { text: "+10s"; compact: true; secondary: true; onClicked: Player.seekBy(10000) }
                AppButton { text: Player.muted ? "Unmute" : "Mute"; compact: true; secondary: true; onClicked: Player.toggleMuted() }
                Text { text: root.time(Player.position) + " / " + root.time(Player.duration); color: Theme.text; font.pixelSize: 12 }
                Item { Layout.fillWidth: true }
                AppButton { visible: Player.position >= Player.introStart && Player.position < Player.introEnd; text: "Skip intro"; compact: true; onClicked: Player.skipIntro() }
                AppButton { visible: Player.position >= Player.outroStart && Player.position < Player.outroEnd; text: "Skip outro"; compact: true; onClicked: Player.skipOutro() }
                ComboBox { visible: Player.captions.length > 0; model: Player.captions; textRole: "label"; Accessible.name: "Caption track"; onActivated: Player.selectCaption(currentIndex) }
                ComboBox { model: ["auto", "1080p", "720p", "480p"]; currentIndex: Math.max(0, model.indexOf(Player.quality)); onActivated: Player.quality = currentText; Accessible.name: "Playback quality" }
                ComboBox { model: [0.5, 0.75, 1, 1.25, 1.5, 2]; currentIndex: 2; onActivated: Player.speed = Number(currentText); Accessible.name: "Playback speed" }
                AppButton { text: "SUB/DUB"; compact: true; secondary: true; onClicked: Player.switchAudio(Player.current.audioMode === "dub" ? "sub" : "dub") }
                AppButton { text: "HD1/HD2"; compact: true; secondary: true; onClicked: Player.switchServer(Player.current.server === "hd-2" ? "hd-1" : "hd-2") }
                AppButton { text: "Next"; compact: true; secondary: true; onClicked: Player.nextEpisode() }
                AppButton { text: "Close"; compact: true; secondary: true; onClicked: Player.close() }
            }
        }
        Text { anchors.left: parent.left; anchors.top: parent.top; anchors.margins: 25; text: (Player.current.animeName || "AniCloud") + "  •  " + (Player.current.episodeName || ("Episode " + (Player.current.episodeNumber || ""))); color: Theme.text; font.pixelSize: 17; font.bold: true }
    }
    Column {
        anchors.centerIn: parent; spacing: 14; visible: Player.state === "loading" || Player.state === "resolving" || Player.state === "buffering"
        BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: parent.visible; palette.accent: Theme.red }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: Player.state === "resolving" ? "Resolving native stream…" : "Buffering…"; color: Theme.text }
    }
    Column {
        anchors.centerIn: parent; spacing: 14; visible: Player.state === "error"
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: Player.error; color: Theme.text; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; width: 500 }
        AppButton { anchors.horizontalCenter: parent.horizontalCenter; text: "Retry"; onClicked: Player.retry() }
    }

    Shortcut { sequence: "Space"; onActivated: Player.togglePlayback() }
    Shortcut { sequence: "Left"; onActivated: Player.seekBy(-10000) }
    Shortcut { sequence: "Right"; onActivated: Player.seekBy(10000) }
    Shortcut { sequence: "Up"; onActivated: Player.adjustVolume(0.05) }
    Shortcut { sequence: "Down"; onActivated: Player.adjustVolume(-0.05) }
    Shortcut { sequence: "M"; onActivated: Player.toggleMuted() }
    Shortcut { sequence: "C"; onActivated: Player.captionsEnabled = !Player.captionsEnabled }
    Shortcut { sequence: "Escape"; onActivated: Player.close() }
    function time(ms) { const total = Math.max(0, Math.floor(ms / 1000)); return Math.floor(total / 60) + ":" + String(total % 60).padStart(2, "0") }
}
