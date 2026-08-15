import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Rectangle {
    id: root
    property bool compact: false
    property string currentRoute: "home"
    readonly property bool expanded: compact || hover.hovered || navigationFocus.activeFocus
    readonly property real preferredWidth: compact ? parent.width : (expanded ? 224 : 72)
    signal navigate(string route)

    color: "#F009090B"
    border.color: Theme.border
    clip: true

    readonly property var entries: [
        { route: "home", icon: "home", label: "Home" },
        { route: "discover", icon: "discover", label: "Discover" },
        { route: "downloads", icon: "download", label: "Downloads" },
        { route: "library", icon: "library", label: "Library" },
        { route: "profile", icon: "profile", label: "Profile" }
    ]
    readonly property var bottomEntries: [
        { route: "home", icon: "home", label: "Home" },
        { route: "discover", icon: "discover", label: "Discover" },
        { route: "downloads", icon: "download", label: "Downloads" },
        { route: "library", icon: "library", label: "Library" },
        { route: "profile", icon: "profile", label: "Profile" },
        { route: "notifications", icon: "notification", label: "Notifications" }
    ]

    HoverHandler { id: hover; acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad }

    FocusScope {
        id: navigationFocus
        anchors.fill: parent
        Loader { anchors.fill: parent; sourceComponent: root.compact ? bottomComponent : railComponent }
    }

    Component {
        id: railComponent
        ColumnLayout {
            anchors.fill: parent
            anchors.topMargin: 16; anchors.bottomMargin: 14
            anchors.leftMargin: 10; anchors.rightMargin: 10
            spacing: 8

            Repeater {
                model: root.entries
                delegate: Button {
                    required property var modelData
                    readonly property bool selected: root.currentRoute.startsWith(modelData.route)
                    Layout.fillWidth: true; implicitHeight: 50
                    focusPolicy: Qt.TabFocus
                    Accessible.name: modelData.label
                    Accessible.description: "Open " + modelData.label
                    contentItem: Item {
                        Rectangle {
                            id: iconTile
                            x: root.expanded ? 7 : (parent.width - width) / 2
                            anchors.verticalCenter: parent.verticalCenter
                            width: 36; height: 36; radius: 11
                            color: parent.parent.selected ? "#28E50914" : "transparent"
                            border.width: parent.parent.selected ? 1 : 0
                            border.color: Theme.red
                            Image {
                                anchors.centerIn: parent; width: 21; height: 21
                                source: Qt.resolvedUrl("../../resources/icons/" + modelData.icon + ".svg")
                                sourceSize.width: 24; sourceSize.height: 24
                                opacity: parent.parent.parent.selected ? 1 : 0.62
                            }
                        }
                        Text {
                            anchors.left: iconTile.right; anchors.leftMargin: 10; anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter; text: modelData.label
                            color: parent.parent.selected ? Theme.text : Theme.muted
                            font.pixelSize: 14; font.weight: Font.DemiBold
                            opacity: root.expanded ? 1 : 0; visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                    }
                    background: Rectangle {
                        radius: Theme.radius
                        color: parent.selected && root.expanded ? Theme.raised : (parent.hovered ? Theme.surface : "transparent")
                    }
                    ToolTip.visible: hovered && !root.expanded
                    ToolTip.text: modelData.label
                    onClicked: root.navigate(modelData.route)
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                readonly property bool selected: root.currentRoute === "notifications"
                Layout.fillWidth: true; implicitHeight: 50
                focusPolicy: Qt.TabFocus
                Accessible.name: "Notifications"
                contentItem: Item {
                    Rectangle {
                        id: notificationTile
                        x: root.expanded ? 7 : (parent.width - width) / 2
                        anchors.verticalCenter: parent.verticalCenter
                        width: 36; height: 36; radius: 11
                        color: parent.parent.selected ? "#28E50914" : "transparent"
                        border.width: parent.parent.selected ? 1 : 0; border.color: Theme.red
                        Image {
                            anchors.centerIn: parent; width: 22; height: 22
                            source: Qt.resolvedUrl("../../resources/icons/notification.svg")
                            sourceSize.width: 24; sourceSize.height: 24
                            opacity: parent.parent.parent.selected ? 1 : 0.62
                        }
                        Rectangle {
                            anchors.right: parent.right; anchors.top: parent.top
                            width: 7; height: 7; radius: 4; color: Theme.red
                            visible: Account.broadcasts.some(item => !item.read)
                        }
                    }
                    Text {
                        anchors.left: notificationTile.right; anchors.leftMargin: 10; anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter; text: "Notifications"
                        color: parent.parent.selected ? Theme.text : Theme.muted
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        opacity: root.expanded ? 1 : 0; visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                }
                background: Rectangle {
                    radius: Theme.radius
                    color: parent.selected && root.expanded ? Theme.raised : (parent.hovered ? Theme.surface : "transparent")
                }
                ToolTip.visible: hovered && !root.expanded
                ToolTip.text: "Notifications"
                onClicked: root.navigate("notifications")
            }
        }
    }

    Component {
        id: bottomComponent
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 6; anchors.rightMargin: 6; spacing: 0
            Repeater {
                model: root.bottomEntries
                delegate: Button {
                    required property var modelData
                    Layout.fillWidth: true; Layout.fillHeight: true
                    focusPolicy: Qt.TabFocus
                    Accessible.name: modelData.label
                    contentItem: Column {
                        anchors.centerIn: parent; spacing: 3
                        Image {
                            anchors.horizontalCenter: parent.horizontalCenter; width: 20; height: 20
                            source: Qt.resolvedUrl("../../resources/icons/" + modelData.icon + ".svg")
                            opacity: root.currentRoute.startsWith(modelData.route) ? 1 : 0.58
                        }
                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 18; height: 2; radius: 1; color: Theme.red
                            visible: root.currentRoute.startsWith(modelData.route)
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter; text: modelData.label
                            color: root.currentRoute.startsWith(modelData.route) ? Theme.text : Theme.muted
                            font.pixelSize: 9
                        }
                    }
                    background: Rectangle { color: "transparent" }
                    onClicked: root.navigate(modelData.route)
                }
            }
        }
    }
}
