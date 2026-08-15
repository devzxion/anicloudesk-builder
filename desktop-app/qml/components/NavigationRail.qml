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
                    Layout.fillWidth: true; implicitHeight: 50
                    focusPolicy: Qt.TabFocus
                    Accessible.name: modelData.label
                    Accessible.description: "Open " + modelData.label
                    contentItem: RowLayout {
                        spacing: 4
                        Item {
                            Layout.preferredWidth: 50; Layout.fillHeight: true
                            Image {
                                anchors.centerIn: parent; width: 22; height: 22
                                source: Qt.resolvedUrl("../../resources/icons/" + modelData.icon + ".svg")
                                sourceSize.width: 24; sourceSize.height: 24
                                opacity: root.currentRoute.startsWith(modelData.route) ? 1 : 0.62
                            }
                        }
                        Text {
                            Layout.fillWidth: true; text: modelData.label
                            color: root.currentRoute.startsWith(modelData.route) ? Theme.text : Theme.muted
                            font.pixelSize: 14; font.weight: Font.DemiBold
                            opacity: root.expanded ? 1 : 0; visible: opacity > 0
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }
                    }
                    background: Rectangle {
                        radius: Theme.radius
                        color: root.currentRoute.startsWith(modelData.route) ? Theme.raised : (parent.hovered ? Theme.surface : "transparent")
                        Rectangle {
                            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                            width: 3; height: 22; radius: 2; color: Theme.red
                            visible: root.currentRoute.startsWith(modelData.route)
                        }
                    }
                    ToolTip.visible: hovered && !root.expanded
                    ToolTip.text: modelData.label
                    onClicked: root.navigate(modelData.route)
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                Layout.fillWidth: true; implicitHeight: 50
                focusPolicy: Qt.TabFocus
                Accessible.name: "Notifications"
                contentItem: RowLayout {
                    spacing: 4
                    Item {
                        Layout.preferredWidth: 50; Layout.fillHeight: true
                        Image {
                            anchors.centerIn: parent; width: 22; height: 22
                            source: Qt.resolvedUrl("../../resources/icons/notification.svg")
                            sourceSize.width: 24; sourceSize.height: 24
                            opacity: root.currentRoute === "notifications" ? 1 : 0.62
                        }
                        Rectangle {
                            anchors.right: parent.right; anchors.rightMargin: 8
                            anchors.top: parent.top; anchors.topMargin: 10
                            width: 7; height: 7; radius: 4; color: Theme.red
                            visible: Account.broadcasts.some(item => !item.read)
                        }
                    }
                    Text {
                        Layout.fillWidth: true; text: "Notifications"
                        color: root.currentRoute === "notifications" ? Theme.text : Theme.muted
                        font.pixelSize: 14; font.weight: Font.DemiBold
                        opacity: root.expanded ? 1 : 0; visible: opacity > 0
                        Behavior on opacity { NumberAnimation { duration: 120 } }
                    }
                }
                background: Rectangle {
                    radius: Theme.radius
                    color: root.currentRoute === "notifications" ? Theme.raised : (parent.hovered ? Theme.surface : "transparent")
                    Rectangle {
                        anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
                        width: 3; height: 22; radius: 2; color: Theme.red
                        visible: root.currentRoute === "notifications"
                    }
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
