import QtQuick
import QtQuick.Controls
import AniCloud

Button {
    id: root
    property string iconName: "minimize"
    property bool destructive: false
    implicitWidth: 48
    implicitHeight: 42
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    contentItem: Item {
        Image {
            anchors.centerIn: parent
            source: Qt.resolvedUrl("../../resources/icons/window-" + root.iconName + ".svg")
            sourceSize.width: 16; sourceSize.height: 16
            width: 16; height: 16; fillMode: Image.PreserveAspectFit
            opacity: root.enabled ? 0.92 : 0.38
        }
    }
    background: Rectangle {
        color: root.down ? (root.destructive ? Theme.darkRed : "#34FFFFFF")
                         : root.hovered || root.activeFocus ? (root.destructive ? Theme.red : "#20FFFFFF")
                         : "transparent"
        border.width: root.activeFocus ? 1 : 0
        border.color: "#88FFFFFF"
        Behavior on color { ColorAnimation { duration: 80 } }
    }
}
