import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AniCloud

Flickable {
    id: root; contentWidth: width; contentHeight: content.implicitHeight + 50; clip: true; ScrollBar.vertical: ScrollBar {}
    ColumnLayout {
        id: content; width: root.width; spacing: 20
        anchors.left: parent.left; anchors.right: parent.right; anchors.margins: 32
        Text { text: "Profile & Settings"; color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black; Layout.topMargin: 28 }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 86; radius: Theme.radius; color: Theme.surface; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 16
                Rectangle { width: 50; height: 50; radius: 25; color: Theme.red; Text { anchors.centerIn: parent; text: Account.authenticated ? String(Account.user.name || "A").charAt(0).toUpperCase() : "G"; color: "white"; font.pixelSize: 22; font.bold: true } }
                ColumnLayout { Layout.fillWidth: true; Text { text: Account.authenticated ? (Account.user.name || "AniCloud user") : "Guest"; color: Theme.text; font.bold: true } Text { text: Account.authenticated ? (Account.user.email || "Signed in") : "Browsing and local history are available"; color: Theme.muted; font.pixelSize: 12 } }
                AppButton { text: Account.authenticated ? "Log out" : "Sign in"; secondary: Account.authenticated; onClicked: Account.authenticated ? Account.logout() : Runtime.route = "auth" }
            }
        }
        Text { text: "Playback"; color: Theme.text; font.pixelSize: 19; font.bold: true }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 150; radius: Theme.radius; color: Theme.surface
            GridLayout {
                anchors.fill: parent; anchors.margins: 16; columns: 2; rowSpacing: 12; columnSpacing: 18
                Text { text: "Default audio"; color: Theme.text }
                ComboBox { model: ["sub", "dub"]; currentIndex: Runtime.audioPreference === "dub" ? 1 : 0; onActivated: Runtime.audioPreference = currentText; Accessible.name: "Default audio" }
                Text { text: "Playback quality"; color: Theme.text }
                ComboBox {
                    model: ["auto", "1080p", "720p", "480p"]
                    currentIndex: Math.max(0, model.indexOf(Runtime.playbackQuality))
                    onActivated: {
                        Runtime.playbackQuality = currentText
                        if (Account.authenticated)
                            Account.setPlaybackPreference({ mode: currentText === "auto" ? "auto" : "height", height: currentText === "auto" ? null : Number(currentText.replace("p", "")) })
                    }
                    Accessible.name: "Playback quality"
                }
                Text { text: "Download quality"; color: Theme.text }
                ComboBox { model: [1080, 720, 480, 360]; currentIndex: Math.max(0, model.indexOf(Runtime.downloadQuality)); onActivated: Runtime.downloadQuality = currentText; Accessible.name: "Download quality" }
            }
        }
        Text { text: "Downloads & notifications"; color: Theme.text; font.pixelSize: 19; font.bold: true }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 170; radius: Theme.radius; color: Theme.surface
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 14
                CheckBox { text: "Allow downloads on metered networks"; checked: Runtime.allowMeteredDownloads; onToggled: Runtime.allowMeteredDownloads = checked; palette.windowText: Theme.text }
                CheckBox { text: "Native broadcast notifications"; checked: Runtime.notificationsEnabled; onToggled: Runtime.notificationsEnabled = checked; palette.windowText: Theme.text }
                RowLayout { Layout.fillWidth: true; Text { text: Downloads.storageRoot; color: Theme.muted; elide: Text.ElideMiddle; Layout.fillWidth: true } AppButton { text: Downloads.movingStorage ? "Moving…" : "Move library"; compact: true; enabled: !Downloads.movingStorage; onClicked: folderDialog.open() } }
            }
        }
        Text { text: "About & updates"; color: Theme.text; font.pixelSize: 19; font.bold: true }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 105; radius: Theme.radius; color: Theme.surface
            RowLayout { anchors.fill: parent; anchors.margins: 16; ColumnLayout { Layout.fillWidth: true; Text { text: "AniCloud Native Desktop 4.0.0"; color: Theme.text; font.bold: true } Text { text: Updates.error || (Updates.status === "current" ? "You are up to date." : "Signed updates from GitHub Releases"); color: Updates.error ? Theme.red : Theme.muted; font.pixelSize: 12 } } AppButton { text: Updates.checking ? "Checking…" : "Check for updates"; enabled: !Updates.checking; onClicked: Updates.check() } }
        }
        RowLayout { AppButton { text: "Privacy"; secondary: true; onClicked: Qt.openUrlExternally("https://anicloud.ink/privacy") } AppButton { text: "Terms"; secondary: true; onClicked: Qt.openUrlExternally("https://anicloud.ink/terms") } }
    }
    FolderDialog { id: folderDialog; title: "Choose a parent folder for the AniCloud library"; onAccepted: Downloads.moveStorage(selectedFolder.toString()) }
    Connections {
        target: Account
        function onPreferenceChanged() {
            if (!Account.playbackPreference.mode) return
            Runtime.playbackQuality = Account.playbackPreference.mode === "auto" ? "auto" : String(Account.playbackPreference.height || 1080) + "p"
        }
    }
}
