import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Item {
    id: root
    property string selectedAnimeKey: ""
    readonly property bool atRefreshBoundary: selectedAnimeKey.length > 0 ? episodeList.atYBeginning : animeList.atYBeginning
    readonly property var selectedGroup: {
        for (const group of Downloads.groups) {
            if (String(group.key) === selectedAnimeKey) return group
        }
        return null
    }

    function stateLabel(item) {
        if (item.state === "completed") return "Available offline"
        if (item.state === "downloading") return "Downloading  " + Math.round(Number(item.progress || 0) * 100) + "%"
        if (item.state === "preparing" || item.state === "queued") return "Preparing download"
        if (item.state === "validating") return "Checking downloaded files"
        if (item.state === "paused") return "Paused"
        if (item.state === "failed") return item.failure || "Download failed"
        return item.state || "Waiting"
    }

    function refreshPage() { Downloads.reload() }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 18
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            AppButton {
                visible: root.selectedAnimeKey.length > 0
                text: "Back"; compact: true; secondary: true
                onClicked: root.selectedAnimeKey = ""
            }
            ColumnLayout {
                Layout.fillWidth: true; spacing: 2
                Text {
                    text: root.selectedGroup ? (root.selectedGroup.animeName || "Downloaded episodes") : "Downloaded anime"
                    color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black
                    elide: Text.ElideRight; Layout.fillWidth: true
                }
                Text {
                    visible: root.selectedGroup !== null
                    text: root.selectedGroup ? root.selectedGroup.completedCount + " of " + root.selectedGroup.episodeCount + " episodes available offline" : ""
                    color: Theme.muted; font.pixelSize: 12
                }
            }
            Text { text: Downloads.storageRoot; color: Theme.muted; font.pixelSize: 11; elide: Text.ElideMiddle; Layout.maximumWidth: 360 }
        }

        Rectangle {
            visible: !Account.authenticated; Layout.fillWidth: true; Layout.preferredHeight: 76
            color: Theme.surface; radius: Theme.radius; border.color: Theme.border
            RowLayout {
                anchors.fill: parent; anchors.margins: 16
                Text { text: "Sign in to download episodes for offline playback."; color: Theme.text; Layout.fillWidth: true }
                AppButton { text: "Sign in"; onClicked: Runtime.route = "auth" }
            }
        }
        Text { visible: Downloads.error.length > 0; text: Downloads.error; color: Theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }

        ListView {
            id: animeList
            visible: root.selectedAnimeKey.length === 0
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 12; clip: true
            model: Downloads.groups
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: animeList.width; height: 124; radius: Theme.radius
                color: groupHover.hovered ? Theme.raised : Theme.surface; border.color: Theme.border
                HoverHandler { id: groupHover }
                RowLayout {
                    anchors.fill: parent; anchors.margins: 12; spacing: 16
                    Image {
                        id: groupPoster
                        source: modelData.animeImage || ""; Layout.preferredWidth: 72; Layout.fillHeight: true
                        fillMode: Image.PreserveAspectCrop; asynchronous: true
                        Rectangle { anchors.fill: parent; color: Theme.raised; visible: groupPoster.status !== Image.Ready }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 6
                        Text { text: modelData.animeName || "Anime"; color: Theme.text; font.pixelSize: 16; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text {
                            text: modelData.completedCount + " of " + modelData.episodeCount + " offline" + (modelData.activeCount > 0 ? "  •  " + modelData.activeCount + " downloading" : "")
                            color: Theme.muted; font.pixelSize: 12
                        }
                        ProgressBar {
                            from: 0; to: 1; value: modelData.progress || 0; Layout.fillWidth: true
                            visible: modelData.completedCount < modelData.episodeCount; palette.highlight: Theme.red
                        }
                    }
                    AppButton { text: "View episodes"; compact: true; onClicked: root.selectedAnimeKey = String(modelData.key) }
                }
            }
        }

        ListView {
            id: episodeList
            visible: root.selectedAnimeKey.length > 0
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 10; clip: true
            model: root.selectedGroup ? root.selectedGroup.episodes : []
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: episodeList.width; height: 112; radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                RowLayout {
                    anchors.fill: parent; anchors.margins: 14; spacing: 14
                    Rectangle {
                        Layout.preferredWidth: 48; Layout.preferredHeight: 48; radius: 24; color: "#25E50914"
                        Text { anchors.centerIn: parent; text: modelData.episodeNumber; color: Theme.red; font.bold: true }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 5
                        Text { text: modelData.episodeName || ("Episode " + modelData.episodeNumber); color: Theme.text; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: "Episode " + modelData.episodeNumber + "  •  " + String(modelData.audioMode || "sub").toUpperCase() + "  •  " + String(modelData.server || "hd-2").toUpperCase(); color: Theme.muted; font.pixelSize: 12 }
                        ProgressBar { from: 0; to: 1; value: modelData.progress || 0; Layout.fillWidth: true; visible: modelData.state !== "completed"; palette.highlight: Theme.red }
                        Text { text: root.stateLabel(modelData); color: modelData.state === "failed" ? Theme.red : Theme.muted; font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    AppButton {
                        text: modelData.state === "completed" ? "Play offline" : ["paused", "failed", "cancelled"].indexOf(modelData.state) >= 0 ? "Resume" : "Pause"
                        compact: true
                        onClicked: {
                            if (modelData.state === "completed") Player.openOffline(modelData)
                            else if (["paused", "failed", "cancelled"].indexOf(modelData.state) >= 0) Downloads.resume(modelData.id)
                            else Downloads.pause(modelData.id)
                        }
                    }
                    AppButton { text: "Delete"; compact: true; secondary: true; onClicked: Downloads.remove(modelData.id) }
                }
            }
        }

        EmptyState {
            visible: Account.authenticated && !Downloads.preparing && Downloads.groups.length === 0
            title: "No downloads yet"; message: "Choose an episode from its details page to add it to your managed offline library."
            Layout.fillWidth: true; Layout.fillHeight: true
        }
    }

    Connections {
        target: Downloads
        function onItemsChanged() {
            if (root.selectedAnimeKey.length > 0 && root.selectedGroup === null) root.selectedAnimeKey = ""
        }
    }
}
