import QtQuick
import QtQuick.Layouts
import AniCloud

Item {
    id: root
    property int rows: 3
    property bool active: false
    property bool shown: false
    property bool coverPage: false
    property string message: "Loading anime…"
    implicitHeight: coverPage ? 0 : skeletons.implicitHeight
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
    Rectangle { anchors.fill: parent; color: Theme.background; visible: root.coverPage }
    ColumnLayout {
        id: skeletons
        width: root.coverPage ? Math.min(760, Math.max(280, root.width - 64)) : root.width
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: root.coverPage ? parent.verticalCenter : null
        spacing: 14
        RowLayout {
            Layout.fillWidth: true; spacing: 12
            BusyIndicator { running: root.visible; implicitWidth: 28; implicitHeight: 28; palette.accent: Theme.red }
            Text { text: root.message; color: Theme.text; font.pixelSize: 17; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
        }
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
