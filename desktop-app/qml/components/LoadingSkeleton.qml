import QtQuick
import QtQuick.Layouts
import AniCloud

Item {
    id: root
    property int rows: 3
    property bool active: false
    property bool shown: false
    implicitHeight: skeletons.implicitHeight
    opacity: shown ? 1 : 0
    visible: opacity > 0
    Accessible.name: "Loading content"
    Accessible.ignored: !visible
    Behavior on opacity { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
    onActiveChanged: {
        if (active) showDelay.restart()
        else {
            showDelay.stop()
            shown = false
        }
    }
    Component.onCompleted: if (active) showDelay.restart()
    Timer { id: showDelay; interval: 160; onTriggered: root.shown = root.active }
    ColumnLayout {
        id: skeletons
        anchors.left: parent.left; anchors.right: parent.right
        spacing: 14
        Repeater {
            model: root.rows
            Rectangle {
                required property int index
                Layout.fillWidth: true
                Layout.preferredHeight: index === 0 ? 190 : 78
                radius: Theme.radius
                color: index % 2 ? Theme.surface : Theme.raised
                border.width: 1; border.color: Theme.border
                opacity: 0.72
            }
        }
    }
}
