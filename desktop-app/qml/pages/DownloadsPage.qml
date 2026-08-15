import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Item {
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 18
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Downloads"; color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black; Layout.fillWidth: true }
            Text { text: Downloads.storageRoot; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.maximumWidth: 420 }
        }
        Rectangle {
            visible: !Account.authenticated; Layout.fillWidth: true; Layout.preferredHeight: 76; color: Theme.surface; radius: Theme.radius; border.color: Theme.border
            RowLayout { anchors.fill: parent; anchors.margins: 16; Text { text: "Sign in to download episodes for offline playback."; color: Theme.text; Layout.fillWidth: true } AppButton { text: "Sign in"; onClicked: Runtime.route = "auth" } }
        }
        Text { visible: Downloads.error.length > 0; text: Downloads.error; color: Theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        ListView {
            id: list; Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10; clip: true; model: Downloads.items
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: list.width; height: 104; radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 14
                    Image { source: modelData.animeImage || ""; Layout.preferredWidth: 58; Layout.fillHeight: true; fillMode: Image.PreserveAspectCrop }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: modelData.animeName || "Anime"; color: Theme.text; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: (modelData.episodeName || ("Episode " + modelData.episodeNumber)) + "  •  " + String(modelData.audioMode || "sub").toUpperCase() + "  •  " + (modelData.qualityHeight || "Auto") + "p"; color: Theme.muted; font.pixelSize: 12 }
                        ProgressBar { from: 0; to: 1; value: modelData.progress || 0; Layout.fillWidth: true; visible: modelData.state !== "completed"; palette.highlight: Theme.red }
                        Text { text: modelData.failure || modelData.state; color: modelData.state === "failed" ? Theme.red : Theme.muted; font.pixelSize: 11 }
                    }
                    AppButton { text: modelData.state === "completed" ? "Play" : ["paused", "failed", "cancelled"].indexOf(modelData.state) >= 0 ? "Resume" : "Pause"; compact: true; onClicked: { if (modelData.state === "completed") Player.openOffline(modelData); else if (["paused", "failed", "cancelled"].indexOf(modelData.state) >= 0) Downloads.resume(modelData.id); else Downloads.pause(modelData.id) } }
                    AppButton { text: "Delete"; compact: true; secondary: true; onClicked: Downloads.remove(modelData.id) }
                }
            }
        }
        EmptyState { visible: Account.authenticated && list.count === 0; title: "No downloads yet"; message: "Choose an episode from its details page to add it to your managed offline library."; Layout.fillWidth: true; Layout.fillHeight: true }
    }
}
