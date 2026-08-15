import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Item {
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 18
        RowLayout { Layout.fillWidth: true; Text { text: "Broadcasts"; color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black; Layout.fillWidth: true } AppButton { text: "Refresh"; compact: true; secondary: true; onClicked: Account.refreshBroadcasts() } }
        ListView {
            id: list; Layout.fillWidth: true; Layout.fillHeight: true; model: Account.broadcasts; spacing: 10; clip: true; ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                required property var modelData
                width: list.width; height: Math.max(105, messageText.implicitHeight + 62); radius: Theme.radius; color: modelData.read ? Theme.surface : Theme.raised; border.color: modelData.read ? Theme.border : Theme.red
                ColumnLayout {
                    anchors.fill: parent; anchors.margins: 15; spacing: 7
                    RowLayout { Layout.fillWidth: true; Text { text: modelData.title || "AniCloud update"; color: Theme.text; font.pixelSize: 16; font.bold: true; Layout.fillWidth: true } Rectangle { visible: !modelData.read; width: 8; height: 8; radius: 4; color: Theme.red } }
                    Text { id: messageText; text: modelData.message || ""; color: Theme.muted; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    AppButton { visible: (modelData.linkUrl || "").length > 0; text: "Open"; compact: true; secondary: true; onClicked: Qt.openUrlExternally(modelData.linkUrl) }
                }
                MouseArea { anchors.fill: parent; acceptedButtons: Qt.LeftButton; propagateComposedEvents: true; onClicked: Account.markBroadcastRead(modelData.id) }
            }
        }
        EmptyState { visible: list.count === 0; title: "No broadcasts"; message: "Service announcements will appear here."; Layout.fillWidth: true; Layout.fillHeight: true }
    }
}
