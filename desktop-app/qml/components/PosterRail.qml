import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

ColumnLayout {
    id: root
    property string title: ""
    property var model: []
    signal activated(var anime)
    spacing: 12
    Text { text: root.title; color: Theme.text; font.pixelSize: 21; font.weight: Font.Bold; Layout.leftMargin: 2 }
    ListView {
        id: rail
        Layout.fillWidth: true
        Layout.preferredHeight: 270
        orientation: ListView.Horizontal
        spacing: 16
        clip: true
        model: root.model
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        delegate: AnimeCard { anime: modelData; onActivated: value => root.activated(value) }
    }
}
