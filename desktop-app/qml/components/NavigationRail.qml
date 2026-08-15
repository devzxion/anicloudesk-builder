import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Rectangle {
    id: root
    property bool compact: false
    property string currentRoute: "home"
    signal navigate(string route)
    color: "#E609090B"
    border.color: Theme.border

    readonly property var entries: [
        { route: "home", icon: "⌂", label: "Home" },
        { route: "discover", icon: "⌕", label: "Discover" },
        { route: "downloads", icon: "⇩", label: "Downloads" },
        { route: "library", icon: "▣", label: "Library" },
        { route: "profile", icon: "●", label: "Profile" }
    ]

    Loader {
        anchors.fill: parent
        sourceComponent: root.compact ? bottomComponent : railComponent
    }

    Component {
        id: railComponent
        ColumnLayout {
            anchors.fill: parent; anchors.margins: 18; spacing: 8
            Text { text: "ANICLOUD"; color: Theme.red; font.pixelSize: 22; font.weight: Font.Black; Layout.bottomMargin: 25 }
            Repeater {
                model: root.entries
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true; implicitHeight: 48
                    Accessible.name: modelData.label
                    contentItem: RowLayout {
                        spacing: 14
                        Text { text: modelData.icon; color: root.currentRoute.startsWith(modelData.route) ? Theme.red : Theme.muted; font.pixelSize: 21; Layout.preferredWidth: 24; horizontalAlignment: Text.AlignHCenter }
                        Text { text: modelData.label; color: root.currentRoute.startsWith(modelData.route) ? Theme.text : Theme.muted; font.pixelSize: 14; font.weight: Font.DemiBold; Layout.fillWidth: true }
                    }
                    background: Rectangle { radius: Theme.radius; color: root.currentRoute.startsWith(modelData.route) ? Theme.raised : (parent.hovered ? Theme.surface : "transparent") }
                    onClicked: root.navigate(modelData.route)
                }
            }
            Item { Layout.fillHeight: true }
            Button {
                Layout.fillWidth: true; text: "Broadcasts"; Accessible.name: text
                contentItem: Text { text: "◉  Broadcasts"; color: Theme.muted; font.pixelSize: 13 }
                background: Rectangle { color: parent.hovered ? Theme.surface : "transparent"; radius: Theme.radius }
                onClicked: root.navigate("broadcasts")
            }
        }
    }

    Component {
        id: bottomComponent
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 8; anchors.rightMargin: 8; spacing: 2
            Repeater {
                model: root.entries
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Accessible.name: modelData.label
                    contentItem: Column {
                        anchors.centerIn: parent; spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.icon; color: root.currentRoute.startsWith(modelData.route) ? Theme.red : Theme.muted; font.pixelSize: 18 }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label; color: Theme.muted; font.pixelSize: 10 }
                    }
                    background: Rectangle { color: "transparent" }
                    onClicked: root.navigate(modelData.route)
                }
            }
        }
    }
}
