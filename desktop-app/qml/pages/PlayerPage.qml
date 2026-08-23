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
    property bool uiLocked: false
    property bool lockButtonVisible: false
    readonly property bool compact: width < 920
    readonly property bool controlsPinned: scrubbing || captionsPopup.opened || captionAppearancePopup.opened || settingsPopup.opened || !Player.playing
    readonly property bool initialBuffering: (Player.state === "loading" || Player.state === "resolving" || Player.state === "buffering") && Player.position < 500
    readonly property bool canSkipIntro: Player.introEnd > Player.introStart && Player.position >= Player.introStart && Player.position < Player.introEnd
    readonly property bool canSkipOutro: Player.outroEnd > Player.outroStart && Player.position >= Player.outroStart && Player.position < Player.outroEnd
    signal toggleFullscreenRequested()
    signal escapeRequested()

    function revealControls() {
        if (uiLocked) {
            revealLockButton()
            return
        }
        controlsVisible = true
        if (Player.playing) hideTimer.restart()
    }
    function revealLockButton() {
        if (!uiLocked) return
        lockButtonVisible = true
        lockButtonTimer.restart()
    }
    function lockPlayerUi() {
        uiLocked = true
        controlsVisible = false
        captionsPopup.close()
        captionAppearancePopup.close()
        settingsPopup.close()
        revealLockButton()
    }
    function unlockPlayerUi() {
        uiLocked = false
        lockButtonVisible = false
        lockHelpPopup.close()
        revealControls()
    }
    function chooseCaption(trackIndex) {
        Player.selectCaption(trackIndex)
        captionsPopup.close()
        revealControls()
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
        cursorShape: root.uiLocked ? (root.lockButtonVisible ? Qt.ArrowCursor : Qt.BlankCursor)
                                   : root.controlsVisible ? Qt.ArrowCursor : Qt.BlankCursor
        onPositionChanged: root.uiLocked ? root.revealLockButton() : root.revealControls()
        onClicked: {
            if (root.uiLocked) {
                root.revealLockButton()
                return
            }
            root.revealControls()
            if (!root.initialBuffering && Player.state !== "error") Player.togglePlayback()
        }
        onDoubleClicked: if (!root.uiLocked) root.toggleFullscreenRequested()
    }

    Timer {
        id: hideTimer
        interval: 4000
        repeat: false
        onTriggered: if (!root.controlsPinned && !root.uiLocked) root.controlsVisible = false
    }

    Timer {
        id: lockButtonTimer
        interval: 2600
        repeat: false
        onTriggered: root.lockButtonVisible = false
    }

    Timer {
        id: lockHintTimer
        interval: 2800
        repeat: false
        onTriggered: lockHelpPopup.close()
    }

    Rectangle {
        id: topShade
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 132; opacity: root.controlsVisible && !root.uiLocked ? 1 : 0; visible: opacity > 0; z: 3
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
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
        opacity: root.controlsVisible && !root.uiLocked && !root.initialBuffering && Player.state !== "error" ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 140 } }
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
        visible: !root.uiLocked && (root.canSkipIntro || root.canSkipOutro)
        AppButton {
            visible: root.canSkipIntro
            text: "Skip intro"; compact: true; onClicked: Player.skipIntro()
        }
        AppButton {
            visible: root.canSkipOutro
            text: "Skip outro"; compact: true; onClicked: Player.skipOutro()
        }
    }

    Rectangle {
        id: bottomShade
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 178; opacity: root.controlsVisible && !root.uiLocked ? 1 : 0; visible: opacity > 0; z: 4
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
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
                ToolTip.visible: pressed
                ToolTip.text: "Seek to " + root.time(value)
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
                Button {
                    visible: !root.compact; implicitWidth: 132; implicitHeight: 40; hoverEnabled: true
                    Accessible.name: "Play next episode"
                    contentItem: Row {
                        anchors.centerIn: parent; spacing: 8
                        Image { source: Qt.resolvedUrl("../../resources/icons/player-next.svg"); sourceSize.width: 20; sourceSize.height: 20; width: 20; height: 20 }
                        Text { text: "Next episode"; color: "white"; font.pixelSize: 12; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
                    }
                    background: Rectangle { radius: 20; color: parent.down ? "#40FFFFFF" : parent.hovered || parent.activeFocus ? "#2BFFFFFF" : "transparent"; border.color: "#5CFFFFFF" }
                    onClicked: Player.nextEpisode()
                    ToolTip.visible: hovered; ToolTip.text: "Next episode (N)"; ToolTip.delay: 500
                }
                PlayerIconButton { visible: root.compact; iconName: "next"; tooltip: "Next episode (N)"; onClicked: Player.nextEpisode() }
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
                    selected: Player.captionsEnabled && Player.selectedCaptionIndex >= 0
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
                PlayerIconButton { iconName: "lock"; tooltip: "Lock player controls"; onClicked: root.lockPlayerUi() }
                PlayerIconButton { iconName: "fullscreen"; tooltip: "Fullscreen (F)"; onClicked: root.toggleFullscreenRequested() }
            }
        }
    }

    Rectangle {
        id: subtitleBackdrop
        readonly property string cue: String(Player.subtitleText || videoOutput.videoSink.subtitleText || "").trim()
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: !root.uiLocked && root.controlsVisible ? 150 : 42
        width: Math.min(parent.width - 80, 900)
        height: subtitleText.implicitHeight + 18
        radius: 6; color: Qt.rgba(0, 0, 0, Runtime.captionBackgroundOpacity); z: 5
        visible: Player.captionsEnabled && cue.length > 0
        Text {
            id: subtitleText
            anchors.fill: parent; anchors.margins: 9
            text: subtitleBackdrop.cue; textFormat: Text.PlainText
            color: Runtime.captionColor; font.pixelSize: (root.compact ? 18 : 22) * Runtime.captionScale; font.weight: Font.DemiBold
            style: Runtime.captionOutline ? Text.Outline : Text.Normal; styleColor: "black"
            wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
        }
    }

    Rectangle {
        id: lockedControl
        anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter; anchors.leftMargin: 28
        width: 58; height: 58; radius: 29; color: "#85000000"; border.color: "#66FFFFFF"; z: 12
        opacity: root.uiLocked && root.lockButtonVisible ? 0.78 : 0
        visible: opacity > 0
        Accessible.role: Accessible.Button
        Accessible.name: "Locked player controls. Double-click to unlock."
        Behavior on opacity { NumberAnimation { duration: 180 } }
        Image {
            anchors.centerIn: parent; width: 25; height: 25
            source: Qt.resolvedUrl("../../resources/icons/player-lock.svg")
            sourceSize.width: 25; sourceSize.height: 25
        }
        MouseArea {
            anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.revealLockButton()
                lockHelpPopup.open()
                lockHintTimer.restart()
            }
            onDoubleClicked: root.unlockPlayerUi()
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
        width: 278; height: Math.min(410, 144 + Player.captions.length * 43)
        modal: false; focus: true; padding: 8
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: root.revealControls()
        background: Rectangle { color: "#F21A1A1E"; radius: 12; border.color: "#4AFFFFFF" }
        contentItem: ColumnLayout {
            spacing: 0
            Text {
                Layout.fillWidth: true; Layout.preferredHeight: 34
                text: Player.captionStatus === "loading" ? "Loading captions…"
                      : Player.captionStatus === "error" ? "Captions could not be loaded" : "Captions"
                color: Player.captionStatus === "error" ? Theme.red : "#BDBDC4"
                font.pixelSize: 12; verticalAlignment: Text.AlignVCenter; leftPadding: 10
            }
            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 42
                text: (!Player.captionsEnabled ? "✓  " : "") + "Captions off"; flat: true
                palette.buttonText: "white"; onClicked: root.chooseCaption(-1)
            }
            ListView {
                id: captionList
                Layout.fillWidth: true; Layout.fillHeight: true; clip: true
                model: Player.captions
                delegate: Button {
                    required property int index
                    required property var modelData
                    width: captionList.width; height: 42; flat: true
                    text: (Player.captionsEnabled && Player.selectedCaptionIndex === index ? "✓  " : "") + (modelData.label || ("Captions " + (index + 1))); palette.buttonText: "white"
                    onClicked: root.chooseCaption(index)
                    Accessible.name: modelData.label || ("Captions " + (index + 1))
                }
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            }
            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#30FFFFFF" }
            Button {
                Layout.fillWidth: true; Layout.preferredHeight: 44; text: "Caption appearance"; flat: true
                palette.buttonText: "#E5E5E8"
                onClicked: { captionsPopup.close(); captionAppearancePopup.open() }
            }
        }
    }

    Popup {
        id: captionAppearancePopup
        x: Math.max(18, root.width - width - 116); y: Math.max(18, root.height - bottomShade.height - height + 18)
        width: 300; height: 354; modal: false; focus: true; padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: root.revealControls()
        background: Rectangle { color: "#F21A1A1E"; radius: 12; border.color: "#4AFFFFFF" }
        contentItem: ColumnLayout {
            spacing: 9
            Text { text: "Caption appearance"; color: "white"; font.pixelSize: 17; font.bold: true }
            Text { text: "Size"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                readonly property var scales: [0.75, 1, 1.25, 1.5]
                Layout.fillWidth: true; model: ["Small", "Medium", "Large", "Extra large"]
                currentIndex: Math.max(0, scales.indexOf(Runtime.captionScale))
                onActivated: Runtime.captionScale = scales[currentIndex]
                Accessible.name: "Caption size"
            }
            Text { text: "Text color"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                readonly property var colors: ["#FFFFFF", "#FFF176", "#80DEEA"]
                Layout.fillWidth: true; model: ["White", "Yellow", "Cyan"]
                currentIndex: Math.max(0, colors.indexOf(Runtime.captionColor))
                onActivated: Runtime.captionColor = colors[currentIndex]
                Accessible.name: "Caption text color"
            }
            Text { text: "Background"; color: "#BDBDC4"; font.pixelSize: 12 }
            ComboBox {
                readonly property var opacityValues: [0, 0.45, 0.72, 0.9]
                Layout.fillWidth: true; model: ["None", "Light", "Dark", "Solid"]
                currentIndex: Math.max(0, opacityValues.indexOf(Runtime.captionBackgroundOpacity))
                onActivated: Runtime.captionBackgroundOpacity = opacityValues[currentIndex]
                Accessible.name: "Caption background"
            }
            CheckBox {
                Layout.fillWidth: true; text: "Text outline"; checked: Runtime.captionOutline
                onToggled: Runtime.captionOutline = checked; palette.windowText: "white"
            }
        }
    }

    Popup {
        id: settingsPopup
        x: Math.max(18, root.width - width - 70); y: Math.max(18, root.height - bottomShade.height - height + 18)
        width: 284; height: 312; modal: false; focus: true; padding: 16
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: root.revealControls()
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

    Popup {
        id: lockHelpPopup
        x: Math.max(20, (root.width - width) / 2)
        y: Math.max(20, root.height * 0.16)
        width: 360; height: 78; modal: false; focus: false; padding: 14; z: 20
        closePolicy: Popup.CloseOnEscape
        background: Rectangle { color: "#ED1A1A1E"; radius: 12; border.color: "#66FFFFFF" }
        contentItem: RowLayout {
            spacing: 12
            Image { source: Qt.resolvedUrl("../../resources/icons/player-lock.svg"); Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
            Text { Layout.fillWidth: true; text: "Player controls are locked. Double-click the lock to unlock."; color: "white"; wrapMode: Text.WordWrap; font.pixelSize: 13 }
        }
    }

    Shortcut { sequence: "Space"; enabled: !root.uiLocked; onActivated: { Player.togglePlayback(); root.revealControls() } }
    Shortcut { sequence: "Left"; enabled: !root.uiLocked; onActivated: { Player.seekBy(-10000); root.revealControls() } }
    Shortcut { sequence: "Right"; enabled: !root.uiLocked; onActivated: { Player.seekBy(10000); root.revealControls() } }
    Shortcut { sequence: "Up"; enabled: !root.uiLocked; onActivated: { Player.adjustVolume(0.05); root.revealControls() } }
    Shortcut { sequence: "Down"; enabled: !root.uiLocked; onActivated: { Player.adjustVolume(-0.05); root.revealControls() } }
    Shortcut { sequence: "M"; enabled: !root.uiLocked; onActivated: { Player.toggleMuted(); root.revealControls() } }
    Shortcut { sequence: "C"; enabled: !root.uiLocked; onActivated: { Player.toggleCaptions(); root.revealControls() } }
    Shortcut { sequence: "N"; enabled: !root.uiLocked; onActivated: { Player.nextEpisode(); root.revealControls() } }
    Shortcut { sequence: "F"; enabled: !root.uiLocked; onActivated: root.toggleFullscreenRequested() }
    Shortcut { sequence: "Escape"; onActivated: root.escapeRequested() }

    Connections {
        target: Player
        function onStateChanged() {
            if (Player.state === "idle") {
                root.uiLocked = false
                root.lockButtonVisible = false
                lockHelpPopup.close()
            } else {
                root.revealControls()
            }
        }
    }
}
