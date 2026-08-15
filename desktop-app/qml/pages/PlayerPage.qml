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
    property bool scrubbing: false
    readonly property bool compact: width < 920
    readonly property bool initialBuffering: (Player.state === "loading" || Player.state === "resolving" || Player.state === "buffering") && Player.position < 500
    signal toggleFullscreenRequested()
    signal escapeRequested()

    function revealControls() {
        controlsVisible = true
        if (Player.playing) hideTimer.restart()
    }
    function time(ms) {
        const total = Math.max(0, Math.floor(ms / 1000))
        const hours = Math.floor(total / 3600)
        const minutes = Math.floor((total % 3600) / 60)
        const seconds = String(total % 60).padStart(2, "0")
        return hours > 0 ? hours + ":" + String(minutes).padStart(2, "0") + ":" + seconds : minutes + ":" + seconds
    }

    VideoOutput {
        id: videoOutput
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectFit
        Component.onCompleted: Player.attachVideoSink(videoSink)
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.controlsVisible ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: root.revealControls()
        onClicked: {
            root.revealControls()
            if (!root.initialBuffering && Player.state !== "error") Player.togglePlayback()
        }
        onDoubleClicked: root.toggleFullscreenRequested()
    }

    Timer {
        id: hideTimer
        interval: 3600
        repeat: false
        onTriggered: if (Player.playing && !root.scrubbing && !captionsPopup.opened && !settingsPopup.opened) root.controlsVisible = false
    }

    Rectangle {
        id: topShade
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 132; visible: root.controlsVisible; z: 3
        gradient: Gradient {
            GradientStop { position: 0; color: "#D9000000" }
            GradientStop { position: 1; color: "#00000000" }
        }
        RowLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
            anchors.leftMargin: 24; anchors.rightMargin: 28; anchors.topMargin: 20; spacing: 14
            PlayerIconButton { iconName: "back"; tooltip: "Close player"; onClicked: Player.close() }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text {
                    Layout.fillWidth: true; text: Player.current.animeName || "AniCloud"
                    color: "white"; font.pixelSize: 17; font.weight: Font.DemiBold; elide: Text.ElideRight
                }
                Text {
                    Layout.fillWidth: true
                    text: (Player.current.episodeNumber ? "Episode " + Player.current.episodeNumber + "  •  " : "") + (Player.current.episodeName || Player.current.title || "")
                    color: "#CFCFD4"; font.pixelSize: 13; elide: Text.ElideRight
                }
            }
            RowLayout {
                visible: Player.state === "buffering" && Player.position >= 500; spacing: 8
                BusyIndicator { running: parent.visible; implicitWidth: 24; implicitHeight: 24; palette.accent: Theme.red }
                Text { text: "Buffering"; color: "#D8D8DC"; font.pixelSize: 12 }
            }
        }
    }

    Row {
        anchors.centerIn: parent; spacing: 24; z: 4
        visible: root.controlsVisible && !root.initialBuffering && Player.state !== "error"
        PlayerIconButton { iconName: "back10"; tooltip: "Back 10 seconds (Left)"; iconSize: 29; onClicked: { Player.seekBy(-10000); root.revealControls() } }
        PlayerIconButton {
            iconName: Player.playing ? "pause" : "play"; tooltip: Player.playing ? "Pause (Space)" : "Play (Space)"
            emphasized: true; iconSize: 29; onClicked: { Player.togglePlayback(); root.revealControls() }
        }
        PlayerIconButton { iconName: "forward10"; tooltip: "Forward 10 seconds (Right)"; iconSize: 29; onClicked: { Player.seekBy(10000); root.revealControls() } }
    }

    Row {
        anchors.right: parent.right; anchors.bottom: bottomShade.top
        anchors.rightMargin: 34; anchors.bottomMargin: 10; spacing: 10; z: 5
        visible: root.controlsVisible
        AppButton {
            visible: Player.introEnd > Player.introStart && Player.position >= Player.introStart && Player.position < Player.introEnd
            text: "Skip intro"; compact: true; onClicked: Player.skipIntro()
        }
        AppButton {
            visible: Player.outroEnd > Player.outroStart && Player.position >= Player.outroStart && Player.position < Player.outroEnd
            text: "Skip outro"; compact: true; onClicked: Player.skipOutro()
        }
    }

    Rectangle {
        id: bottomShade
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 172; visible: root.controlsVisible; z: 4
        gradient: Gradient {
            GradientStop { position: 0; color: "#00000000" }
            GradientStop { position: 1; color: "#E8000000" }
        }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.leftMargin: 28; anchors.rightMargin: 28; anchors.bottomMargin: 20; spacing: 7

            Slider {
                id: seekControl
                Layout.fillWidth: true; Layout.preferredHeight: 24
                from: 0; to: Math.max(1, Player.duration); value: Player.position
                Accessible.name: "Playback position"
                onPressedChanged: { root.scrubbing = pressed; root.revealControls() }
                onMoved: Player.seek(value)
                background: Rectangle {
                    x: seekControl.leftPadding; y: seekControl.topPadding + seekControl.availableHeight / 2 - height / 2
                    width: seekControl.availableWidth; height: seekControl.hovered || seekControl.pressed ? 6 : 4; radius: height / 2
                    color: "#5AFFFFFF"
                    Rectangle {
                        width: parent.width * Math.max(seekControl.visualPosition, Math.min(1, Player.bufferProgress))
                        height: parent.height; radius: parent.radius; color: "#A8FFFFFF"
                    }
                    Rectangle { width: parent.width * seekControl.visualPosition; height: parent.height; radius: parent.radius; color: Theme.red }
                }
                handle: Rectangle {
                    x: seekControl.leftPadding + seekControl.visualPosition * (seekControl.availableWidth - width)
                    y: seekControl.topPadding + seekControl.availableHeight / 2 - height / 2
                    width: seekControl.pressed || seekControl.hovered ? 16 : 12
                    height: width; radius: width / 2; color: Theme.red
                    Behavior on width { NumberAnimation { duration: 80 } }
                }
            }

            RowLayout {
                Layout.fillWidth: true; spacing: 4
                PlayerIconButton { iconName: Player.playing ? "pause" : "play"; tooltip: Player.playing ? "Pause" : "Play"; onClicked: Player.togglePlayback() }
                PlayerIconButton { iconName: Player.muted || Player.volume === 0 ? "muted" : "volume"; tooltip: Player.muted ? "Unmute (M)" : "Mute (M)"; onClicked: Player.toggleMuted() }
                Slider {
                    id: volumeControl
                    visible: !root.compact; Layout.preferredWidth: 92; from: 0; to: 1; value: Player.volume
                    Accessible.name: "Volume"; onMoved: Player.volume = value
                    background: Rectangle {
                        x: volumeControl.leftPadding; y: volumeControl.topPadding + volumeControl.availableHeight / 2 - 2
                        width: volumeControl.availableWidth; height: 4; radius: 2; color: "#55FFFFFF"
                        Rectangle { width: parent.width * volumeControl.visualPosition; height: parent.height; radius: 2; color: "white" }
                    }
                    handle: Rectangle {
                        x: volumeControl.leftPadding + volumeControl.visualPosition * (volumeControl.availableWidth - width)
                        y: volumeControl.topPadding + volumeControl.availableHeight / 2 - height / 2
                        width: 12; height: 12; radius: 6; color: "white"
                    }
                }
                Text {
                    text: root.time(Player.position) + " / " + (Player.duration > 0 ? root.time(Player.duration) : "--:--")
                    color: "white"; font.pixelSize: 12; Layout.leftMargin: 5
                }
                Item { Layout.fillWidth: true }
                PlayerIconButton { iconName: "next"; tooltip: "Next episode"; onClicked: Player.nextEpisode() }
                Button {
                    implicitWidth: 62; implicitHeight: 38; hoverEnabled: true
                    Accessible.name: "Volume booster"
                    contentItem: Text { text: Math.round(Player.volumeBoost * 100) + "%"; color: "white"; font.pixelSize: 12; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { radius: 19; color: parent.hovered ? "#32FFFFFF" : "transparent"; border.color: Player.volumeBoost > 1 ? Theme.red : "#66FFFFFF" }
                    onClicked: Player.cycleVolumeBoost()
                    ToolTip.visible: hovered; ToolTip.text: "Volume boost"; ToolTip.delay: 500
                }
                PlayerIconButton {
                    iconName: "captions"; tooltip: "Captions (C)"; enabled: Player.captions.length > 0
                    onClicked: { root.revealControls(); captionsPopup.open() }
                }
                AppButton {
                    visible: !root.compact; compact: true; secondary: true
                    text: String(Player.current.audioMode || "sub").toUpperCase()
                    onClicked: Player.switchAudio(Player.current.audioMode === "dub" ? "sub" : "dub")
                }
                AppButton {
                    visible: !root.compact; compact: true; secondary: true
                    text: Player.current.server === "hd-2" ? "HD2" : "HD1"
                    onClicked: Player.switchServer(Player.current.server === "hd-2" ? "hd-1" : "hd-2")
                }
                PlayerIconButton { iconName: "settings"; tooltip: "Playback settings"; onClicked: { root.revealControls(); settingsPopup.open() } }
                PlayerIconButton { iconName: "fullscreen"; tooltip: "Fullscreen (F)"; onClicked: root.toggleFullscreenRequested() }
            }
        }
    }

    Column {
        anchors.centerIn: parent; spacing: 14; visible: root.initialBuffering; z: 6
        BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: parent.visible; implicitWidth: 54; implicitHeight: 54; palette.accent: Theme.red }
        Text { anchors.horizontalCenter: parent.horizontalCenter; text: Player.state === "resolving" ? "Finding the best stream…" : "Buffering…"; color: "white"; font.pixelSize: 14 }
    }

    ColumnLayout {
        anchors.centerIn: parent; width: Math.min(520, parent.width - 64); spacing: 16; visible: Player.state === "error"; z: 7
        Text { Layout.fillWidth: true; text: "Playback unavailable"; color: "white"; font.pixelSize: 23; font.bold: true; horizontalAlignment: Text.AlignHCenter }
        Text { Layout.fillWidth: true; text: Player.error; color: "#C6C6CC"; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            AppButton { text: "Retry"; onClicked: Player.retry() }
            AppButton { text: "Close"; secondary: true; onClicked: Player.close() }
        }
    }

    Popup {
        id: captionsPopup
        x: Math.max(18, root.width - width - 116); y: Math.max(18, root.height - bottomShade.height - height + 18)
        width: 230; height: Math.min(310, 58 + Player.captions.length * 43)
        modal: false; focus: true; padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#F21A1A1E"; radius: 12; border.color: "#4AFFFFFF" }
        contentItem: Column {
            Button {
                width: parent.width; height: 42; text: "Captions off"; flat: true
                palette.buttonText: "white"; onClicked: { Player.captionsEnabled = false; captionsPopup.close() }
            }
            Repeater {
                model: Player.captions
                delegate: Button {
                    required property var modelData
                    width: parent.width; height: 42; flat: true
                    text: modelData.label || ("Captions " + (index + 1)); palette.buttonText: "white"
                    onClicked: { Player.selectCaption(index); captionsPopup.close() }
                }
            }
        }
    }

    Popup {
        id: settingsPopup
        x: Math.max(18, root.width - width - 70); y: Math.max(18, root.height - bottomShade.height - height + 18)
        width: 284; height: 312; modal: false; focus: true; padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: "#F21A1A1E"; radius: 12; border.color: "#4AFFFFFF" }
        contentItem: ColumnLayout {
            spacing: 10
            Text { text: "Playback settings"; color: "white"; font.pixelSize: 17; font.bold: true }
            Text { text: "Quality"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                Layout.fillWidth: true; model: ["auto", "1080p", "720p", "480p"]
                currentIndex: Math.max(0, model.indexOf(Player.quality)); onActivated: Player.quality = currentText
                Accessible.name: "Playback quality"
            }
            Text { text: "Speed"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                readonly property var rates: [0.5, 0.75, 1, 1.25, 1.5, 2]
                Layout.fillWidth: true; model: ["0.5×", "0.75×", "Normal", "1.25×", "1.5×", "2×"]
                currentIndex: Math.max(0, rates.indexOf(Player.speed)); onActivated: Player.speed = rates[currentIndex]
                Accessible.name: "Playback speed"
            }
            Text { text: "Volume boost"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                readonly property var boosts: [1, 1.25, 1.5, 2]
                Layout.fillWidth: true; model: ["100%", "125%", "150%", "200%"]
                currentIndex: Math.max(0, boosts.indexOf(Player.volumeBoost)); onActivated: Player.volumeBoost = boosts[currentIndex]
                Accessible.name: "Volume boost"
            }
            RowLayout {
                visible: root.compact; Layout.fillWidth: true
                AppButton { Layout.fillWidth: true; text: String(Player.current.audioMode || "sub").toUpperCase(); secondary: true; compact: true; onClicked: Player.switchAudio(Player.current.audioMode === "dub" ? "sub" : "dub") }
                AppButton { Layout.fillWidth: true; text: Player.current.server === "hd-2" ? "HD2" : "HD1"; secondary: true; compact: true; onClicked: Player.switchServer(Player.current.server === "hd-2" ? "hd-1" : "hd-2") }
            }
        }
    }

    Shortcut { sequence: "Space"; onActivated: { Player.togglePlayback(); root.revealControls() } }
    Shortcut { sequence: "Left"; onActivated: { Player.seekBy(-10000); root.revealControls() } }
    Shortcut { sequence: "Right"; onActivated: { Player.seekBy(10000); root.revealControls() } }
    Shortcut { sequence: "Up"; onActivated: { Player.adjustVolume(0.05); root.revealControls() } }
    Shortcut { sequence: "Down"; onActivated: { Player.adjustVolume(-0.05); root.revealControls() } }
    Shortcut { sequence: "M"; onActivated: { Player.toggleMuted(); root.revealControls() } }
    Shortcut { sequence: "C"; onActivated: { Player.captionsEnabled = !Player.captionsEnabled; root.revealControls() } }
    Shortcut { sequence: "F"; onActivated: root.toggleFullscreenRequested() }
    Shortcut { sequence: "Escape"; onActivated: root.escapeRequested() }

    Connections {
        target: Player
        function onStateChanged() { root.revealControls() }
    }
}
