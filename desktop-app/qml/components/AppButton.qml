import QtQuick
import QtQuick.Controls
import AniCloud

Button {
    id: control
    property bool secondary: false
    property bool compact: false
    implicitHeight: compact ? 34 : 42
    implicitWidth: Math.max(92, contentItem.implicitWidth + 28)
    Accessible.name: text
    contentItem: Text {
        text: control.text
        color: control.enabled ? Theme.text : Theme.muted
        font.pixelSize: control.compact ? 13 : 14
        font.weight: Font.DemiBold
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    background: Rectangle {
        radius: Theme.radius
        color: !control.enabled ? Theme.raised
               : control.down ? (control.secondary ? Theme.border : Theme.darkRed)
               : control.hovered ? (control.secondary ? "#303036" : "#F11A25")
               : control.secondary ? Theme.raised : Theme.red
        border.color: control.secondary ? Theme.border : "transparent"
        Behavior on color { ColorAnimation { duration: 100 } }
    }
}
