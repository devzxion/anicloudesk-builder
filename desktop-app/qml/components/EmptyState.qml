import QtQuick
import QtQuick.Layouts
import AniCloud

ColumnLayout {
    property string symbol: "◇"
    property string title: "Nothing here yet"
    property string message: ""
    spacing: 10
    Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
    Text { text: parent.symbol; color: Theme.red; font.pixelSize: 42; Layout.alignment: Qt.AlignHCenter }
    Text { text: parent.title; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; Layout.alignment: Qt.AlignHCenter }
    Text { text: parent.message; color: Theme.muted; font.pixelSize: 14; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.maximumWidth: 420; Layout.alignment: Qt.AlignHCenter }
}
