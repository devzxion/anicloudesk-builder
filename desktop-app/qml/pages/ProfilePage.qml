import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import AniCloud

Flickable {
    id: root
    contentWidth: width
    contentHeight: content.implicitHeight + 56
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}
    readonly property bool narrow: width < 850

    ColumnLayout {
        id: content
        width: Math.min(980, Math.max(520, root.width - 56))
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 18

        Text {
            text: "Profile & Settings"
            color: Theme.text
            font.pixelSize: Theme.titleSize
            font.weight: Font.Black
            Layout.topMargin: 28
            Layout.bottomMargin: 4
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 96
            radius: Theme.radius; color: Theme.surface; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 15
                Rectangle {
                    Layout.preferredWidth: 54; Layout.preferredHeight: 54; radius: 27
                    color: Theme.red
                    Text {
                        anchors.centerIn: parent
                        text: Account.authenticated ? String(Account.user.name || "A").charAt(0).toUpperCase() : "G"
                        color: "white"; font.pixelSize: 22; font.bold: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 4
                    Text {
                        Layout.fillWidth: true
                        text: Account.authenticated ? (Account.user.name || "AniCloud user") : "Guest profile"
                        color: Theme.text; font.pixelSize: 16; font.bold: true; elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: Account.authenticated ? (Account.user.email || "Signed in") : "Sign in to sync your list, history, and downloads."
                        color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight
                    }
                }
                AppButton {
                    text: Account.authenticated ? "Log out" : "Sign in"
                    secondary: Account.authenticated
                    onClicked: Account.authenticated ? Account.logout() : Runtime.route = "auth"
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: root.narrow ? 1 : 2
            columnSpacing: 16; rowSpacing: 16

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 238
                radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Text { text: "Playback"; color: Theme.text; font.pixelSize: 18; font.bold: true }
                    Text { text: "Choose the defaults used when an episode starts."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.preferredHeight: 2 }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Default audio"; color: Theme.text; Layout.fillWidth: true }
                        ComboBox {
                            Layout.preferredWidth: 158; model: ["Sub", "Dub"]
                            currentIndex: Runtime.audioPreference === "dub" ? 1 : 0
                            onActivated: Runtime.audioPreference = currentIndex === 1 ? "dub" : "sub"
                            Accessible.name: "Default audio"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Playback quality"; color: Theme.text; Layout.fillWidth: true }
                        ComboBox {
                            Layout.preferredWidth: 158; model: ["Auto", "1080p", "720p", "480p"]
                            currentIndex: Math.max(0, model.map(value => value.toLowerCase()).indexOf(Runtime.playbackQuality))
                            onActivated: {
                                Runtime.playbackQuality = currentText.toLowerCase()
                                if (Account.authenticated)
                                    Account.setPlaybackPreference({ mode: Runtime.playbackQuality === "auto" ? "auto" : "height", height: Runtime.playbackQuality === "auto" ? null : Number(Runtime.playbackQuality.replace("p", "")) })
                            }
                            Accessible.name: "Playback quality"
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "Download quality"; color: Theme.text; Layout.fillWidth: true }
                        ComboBox {
                            Layout.preferredWidth: 158; model: ["1080p", "720p", "480p", "360p"]
                            currentIndex: Math.max(0, model.indexOf(String(Runtime.downloadQuality) + "p"))
                            onActivated: Runtime.downloadQuality = Number(currentText.replace("p", ""))
                            Accessible.name: "Download quality"
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true; Layout.preferredHeight: 238
                radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 18; spacing: 12
                    Text { text: "Downloads & notifications"; color: Theme.text; font.pixelSize: 18; font.bold: true }
                    Text { text: "Control background activity and native desktop alerts."; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Item { Layout.preferredHeight: 2 }
                    CheckBox {
                        Layout.fillWidth: true
                        text: "Allow downloads on metered networks"
                        checked: Runtime.allowMeteredDownloads
                        onToggled: Runtime.allowMeteredDownloads = checked
                        palette.windowText: Theme.text
                    }
                    CheckBox {
                        Layout.fillWidth: true
                        text: "Desktop notifications"
                        checked: Runtime.notificationsEnabled
                        onToggled: Runtime.notificationsEnabled = checked
                        palette.windowText: Theme.text
                    }
                    Item { Layout.fillHeight: true }
                    Text {
                        text: Account.authenticated ? "Cloud sync is enabled for this account." : "Sign in to enable cloud history and downloads."
                        color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 104
            radius: Theme.radius; color: Theme.surface; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 18
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 5
                    Text { text: "Offline library"; color: Theme.text; font.pixelSize: 16; font.bold: true }
                    Text {
                        Layout.fillWidth: true; text: Downloads.storageRoot
                        color: Theme.muted; font.pixelSize: 12; elide: Text.ElideMiddle
                    }
                }
                AppButton {
                    text: Downloads.movingStorage ? "Moving…" : "Move library"
                    compact: true; enabled: !Downloads.movingStorage
                    onClicked: folderDialog.open()
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 112
            radius: Theme.radius; color: Theme.surface; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 18
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 5
                    Text { text: "AniCloud Desktop " + Qt.application.version; color: Theme.text; font.pixelSize: 16; font.bold: true }
                    Text {
                        Layout.fillWidth: true
                        text: Updates.error || (Updates.status === "current" ? "You are up to date." : "Updates are verified through GitHub Releases.")
                        color: Updates.error ? Theme.red : Theme.muted; font.pixelSize: 12; wrapMode: Text.WordWrap
                    }
                }
                AppButton { text: Updates.checking ? "Checking…" : "Check for updates"; enabled: !Updates.checking; onClicked: Updates.check() }
            }
        }

        RowLayout {
            spacing: 10; Layout.bottomMargin: 8
            AppButton { text: "Privacy"; secondary: true; compact: true; onClicked: Qt.openUrlExternally("https://anicloud.ink/privacy") }
            AppButton { text: "Terms"; secondary: true; compact: true; onClicked: Qt.openUrlExternally("https://anicloud.ink/terms") }
        }
    }

    FolderDialog {
        id: folderDialog
        title: "Choose a parent folder for the AniCloud library"
        onAccepted: Downloads.moveStorage(selectedFolder.toString())
    }
    Connections {
        target: Account
        function onPreferenceChanged() {
            if (!Account.playbackPreference.mode) return
            Runtime.playbackQuality = Account.playbackPreference.mode === "auto" ? "auto" : String(Account.playbackPreference.height || 1080) + "p"
        }
    }
}
