import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Item {
    id: root
    property int tab: 0
    readonly property var tabNames: ["My List", "History", "Completed", "Resume"]
    readonly property var activeModel: {
        if (tab === 0) return Account.authenticated ? Account.watchlist : []
        if (tab === 1) return Account.authenticated ? Account.history : Runtime.localHistory
        if (tab === 2) return Account.authenticated ? Account.completed : []
        return (Account.authenticated ? Account.history : Runtime.localHistory).filter(item => (item.positionSeconds || item.progressSeconds || 0) > 0)
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Text { text: "Library"; color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black; Layout.fillWidth: true }
            AppButton { text: "Refresh"; compact: true; secondary: true; visible: Account.authenticated; onClicked: Account.refreshLibrary() }
        }
        TabBar {
            id: tabs; currentIndex: root.tab; onCurrentIndexChanged: root.tab = currentIndex
            background: Rectangle { color: "transparent" }
            Repeater {
                model: root.tabNames
                TabButton {
                    required property string modelData
                    text: modelData; palette.buttonText: checked ? Theme.text : Theme.muted
                    background: Rectangle { color: "transparent"; Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 3; color: Theme.red; visible: parent.parent.checked } }
                }
            }
        }
        ListView {
            id: list; Layout.fillWidth: true; Layout.fillHeight: true; model: root.activeModel; spacing: 9; clip: true; ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: list.width; height: 84; radius: Theme.radius; color: Theme.surface; border.color: Theme.border
                RowLayout {
                    anchors.fill: parent; anchors.margins: 11; spacing: 14
                    Image { source: modelData.image || modelData.animeImage || modelData.poster || ""; Layout.preferredWidth: 48; Layout.fillHeight: true; fillMode: Image.PreserveAspectCrop }
                    ColumnLayout {
                        Layout.fillWidth: true
                        Text { text: modelData.title || modelData.animeName || "Anime"; color: Theme.text; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.episodeName || (modelData.episodeNumber ? "Episode " + modelData.episodeNumber : "Saved title"); color: Theme.muted; font.pixelSize: 12 }
                    }
                    AppButton { text: "Open"; compact: true; onClicked: Runtime.route = "details/" + (modelData.animeId || modelData.id) }
                    AppButton { visible: root.tab === 1; text: "Delete"; compact: true; secondary: true; onClicked: Account.authenticated ? Account.deleteHistory(modelData.id) : Runtime.deleteLocalHistory(modelData.animeId, modelData.episodeId) }
                }
            }
        }
        EmptyState { visible: list.count === 0; title: root.tabNames[root.tab] + " is empty"; message: !Account.authenticated && root.tab !== 1 ? "Sign in to sync this part of your library." : "Titles will appear here as you use AniCloud."; Layout.fillWidth: true; Layout.fillHeight: true }
    }
}
