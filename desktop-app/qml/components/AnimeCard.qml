import QtQuick
import QtQuick.Controls
import AniCloud

Item {
    id: root
    property var anime: ({})
    signal activated(var anime)
    width: 150
    height: 260
    Accessible.role: Accessible.Button
    Accessible.name: anime.title || "Anime"
    focus: true

    Rectangle {
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.surface
        border.color: root.activeFocus ? Theme.red : (mouse.containsMouse ? Theme.border : "transparent")
        clip: true
        Image {
            id: poster
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 210
            source: root.anime.poster || root.anime.image || root.anime.animeImage || ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            Rectangle { anchors.fill: parent; color: Theme.raised; visible: poster.status !== Image.Ready }
            Rectangle {
                anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.margins: 7
                radius: 4; color: Theme.red; visible: (root.anime.subEpisodes || 0) > 0
                width: subLabel.implicitWidth + 10; height: 22
                Text { id: subLabel; anchors.centerIn: parent; text: "SUB " + root.anime.subEpisodes; color: "white"; font.pixelSize: 10; font.bold: true }
            }
        }
        Text {
            anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
            anchors.margins: 10; height: 32
            text: root.anime.title || root.anime.animeName || "Untitled"
            color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold
            maximumLineCount: 2; wrapMode: Text.WordWrap; elide: Text.ElideRight
        }
    }
    MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: root.activated(root.anime) }
    Keys.onReturnPressed: activated(anime)
    Keys.onEnterPressed: activated(anime)
    scale: mouse.containsMouse ? 1.035 : 1
    Behavior on scale { NumberAnimation { duration: 130 } }
}
