import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Flickable {
    id: root
    property string animeId: ""
    property int episodeLimit: 12
    property string shareNotice: ""
    contentWidth: width
    contentHeight: content.implicitHeight + 40
    clip: true
    ScrollBar.vertical: ScrollBar {}

    onAnimeIdChanged: if (animeId.length > 0) { episodeLimit = 12; Provider.loadDetails(animeId) }

    ColumnLayout {
        id: content; width: root.width; spacing: 24
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 410; color: Theme.surface; clip: true
            Image { anchors.fill: parent; source: Provider.details.banner || Provider.details.poster || ""; fillMode: Image.PreserveAspectCrop; opacity: 0.48; asynchronous: true }
            Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0; color: "#2209090B" } GradientStop { position: 1; color: Theme.background } } }
            RowLayout {
                anchors.fill: parent; anchors.margins: 34; spacing: 28
                Image { source: Provider.details.poster || ""; Layout.preferredWidth: 190; Layout.preferredHeight: 285; fillMode: Image.PreserveAspectCrop; asynchronous: true }
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 12
                    Text { text: Provider.details.title || "Loading…"; color: Theme.text; font.pixelSize: 34; font.weight: Font.Black; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    Text { text: [Provider.details.type, Provider.details.status, Provider.details.duration].filter(value => value).join("  •  "); color: Theme.muted; font.pixelSize: 14 }
                    Text { text: Provider.details.synopsis || Provider.details.description || ""; color: "#D0F8F8FA"; font.pixelSize: 14; maximumLineCount: 6; elide: Text.ElideRight; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                    RowLayout {
                        AppButton { text: Account.watchlist.some(item => item.animeId === root.animeId) ? "✓ My List" : "+ My List"; enabled: Account.authenticated; onClicked: Account.watchlist.some(item => item.animeId === root.animeId) ? Account.removeFromWatchlist(root.animeId) : Account.addToWatchlist(Provider.details) }
                        AppButton {
                            text: "Share"
                            secondary: true
                            onClicked: Runtime.shareAnime(root.animeId, Provider.details.title || "Anime")
                        }
                        Text {
                            visible: root.shareNotice.length > 0
                            text: root.shareNotice
                            color: "#7EE787"
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.leftMargin: 32; Layout.rightMargin: 32; spacing: 14
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Episodes"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold; Layout.fillWidth: true }
                TextField {
                    id: episodeSearch; placeholderText: "Episode number"; inputMethodHints: Qt.ImhDigitsOnly; color: Theme.text; implicitWidth: 170
                    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
                }
            }
            Repeater {
                model: Provider.episodes.filter(item => !episodeSearch.text.length || String(item.number) === episodeSearch.text).slice(0, root.episodeLimit)
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true; Layout.preferredHeight: 64; color: Theme.surface; radius: Theme.radius
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                        Text { text: "EP " + modelData.number; color: Theme.red; font.bold: true; Layout.preferredWidth: 65 }
                        Text { text: modelData.title || ("Episode " + modelData.number); color: Theme.text; elide: Text.ElideRight; Layout.fillWidth: true }
                        AppButton { text: "Play"; compact: true; onClicked: Player.open(Object.assign({}, modelData, { animeId: root.animeId, animeName: Provider.details.title, animeImage: Provider.details.poster, audioMode: Runtime.audioPreference })) }
                        AppButton { text: "Download"; compact: true; secondary: true; enabled: Account.authenticated; onClicked: Downloads.enqueueEpisode(Object.assign({}, modelData, { animeId: root.animeId, animeName: Provider.details.title, animeImage: Provider.details.poster, audioMode: Runtime.audioPreference }), Runtime.downloadQuality) }
                    }
                }
            }
            AppButton { visible: root.episodeLimit < Provider.episodes.length; text: "Load 12 more"; secondary: true; Layout.alignment: Qt.AlignHCenter; onClicked: root.episodeLimit += 12 }
            PosterRail { title: "You may also like"; model: Provider.recommendations; Layout.fillWidth: true; onActivated: anime => Runtime.route = "details/" + anime.id }
        }
    }
    LoadingSkeleton { anchors.centerIn: parent; width: Math.min(760, parent.width - 64); rows: 4; visible: Provider.loading && !Provider.details.title }
    EmptyState { anchors.centerIn: parent; visible: !Provider.loading && !Provider.details.title && Provider.error.length > 0; title: "Details unavailable"; message: Provider.error; symbol: "!" }
    Timer { id: shareNoticeTimer; interval: 2800; onTriggered: root.shareNotice = "" }
    Connections {
        target: Runtime
        function onShareLinkCopied(url) {
            root.shareNotice = "AniCloud link copied"
            shareNoticeTimer.restart()
        }
    }
}
