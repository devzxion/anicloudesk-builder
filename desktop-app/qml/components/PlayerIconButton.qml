import QtQuick
import QtQuick.Controls
import AniCloud

Button {
    id: root
    property string iconName: "play"
    property string tooltip: ""
    property int iconSize: 23
    property bool emphasized: false
    implicitWidth: emphasized ? 62 : 44
    implicitHeight: emphasized ? 62 : 44
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: tooltip
    contentItem: Image {
        source: Qt.resolvedUrl("../../resources/icons/player-" + root.iconName + ".svg")
        sourceSize.width: root.iconSize; sourceSize.height: root.iconSize
        width: root.iconSize; height: root.iconSize
        anchors.centerIn: parent; fillMode: Image.PreserveAspectFit
        opacity: root.enabled ? 1 : 0.38
    }
    background: Rectangle {
        radius: width / 2
        color: root.emphasized ? (root.down ? "#C40710" : Theme.red)
                               : root.down ? "#55FFFFFF" : root.hovered || root.activeFocus ? "#32FFFFFF" : "transparent"
        border.width: root.activeFocus ? 2 : 0
        border.color: "white"
        Behavior on color { ColorAnimation { duration: 90 } }
    }
    ToolTip.visible: hovered && tooltip.length > 0
    ToolTip.text: tooltip
    ToolTip.delay: 500
}
