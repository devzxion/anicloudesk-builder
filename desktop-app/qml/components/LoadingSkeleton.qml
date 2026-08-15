import QtQuick
import QtQuick.Layouts
import AniCloud

ColumnLayout {
    id: root
    property int rows: 3
    spacing: 14
    Accessible.name: "Loading content"
    Repeater {
        model: root.rows
        Rectangle {
            required property int index
            Layout.fillWidth: true
            Layout.preferredHeight: index === 0 ? 190 : 78
            radius: Theme.radius
            color: index % 2 ? Theme.surface : Theme.raised
            opacity: 0.45
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { from: 0.38; to: 0.82; duration: 650; easing.type: Easing.InOutSine }
                NumberAnimation { from: 0.82; to: 0.38; duration: 650; easing.type: Easing.InOutSine }
            }
        }
    }
}
