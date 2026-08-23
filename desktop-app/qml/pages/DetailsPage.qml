import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Flickable {
    id: root
    property string animeId: ""
    property int episodeLimit: 24
    property string shareNotice: ""
    readonly property var matchingEpisodes: {
        const needle = episodeSearch.text.trim().toLowerCase()
        if (!needle.length) return Provider.episodes
        return Provider.episodes.filter(item => {
            const number = String(item.number || "")
            const title = String(item.episodeName || item.title || "").toLowerCase()
            const alternative = String(item.alternativeTitle || "").toLowerCase()
            return number === needle || title.includes(needle) || alternative.includes(needle)
        })
    }
    function downloadFor(episode) {
        // Referencing these properties keeps every delegate in sync as jobs move
        // from resolving to downloading to completed.
        const records = Downloads.items
        const resolving = Downloads.preparing
        return Downloads.episodeStatus(root.animeId, String(episode.episodeId || episode.id || ""))
    }
    function downloadLabel(record) {
        if (!record || !record.state) return "Download"
        if (record.state === "completed") return "Play offline"
        if (record.state === "downloading") return Math.round(Number(record.progress || 0) * 100) + "% downloaded"
        if (record.state === "preparing" || record.state === "queued") return "Preparing…"
        if (record.state === "validating") return "Validating…"
        if (["paused", "failed", "cancelled"].indexOf(record.state) >= 0) return "Resume"
        return "Downloading…"
    }
    contentWidth: width
    contentHeight: content.implicitHeight + 40
    clip: true
    ScrollBar.vertical: ScrollBar {}

    onAnimeIdChanged: if (animeId.length > 0) { episodeLimit = 24; Provider.loadDetails(animeId) }

    ColumnLayout {
        id: content; width: root.width; spacing: 24
        visible: Provider.details.title
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
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 2
                    Text { text: "Episodes"; color: Theme.text; font.pixelSize: 22; font.weight: Font.Bold }
                    Text {
                        text: Provider.episodes.length > 0 ? Provider.episodes.length + " episodes available" : "Finding available episodes…"
                        color: Theme.muted; font.pixelSize: 12
                    }
                }
                TextField {
                    id: episodeSearch; placeholderText: "Episode number or title"; color: Theme.text; implicitWidth: 240
                    Accessible.name: "Search episodes by number or title"
                    onTextChanged: root.episodeLimit = 24
                    background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: Theme.border }
                }
            }
            Repeater {
                model: root.matchingEpisodes.slice(0, root.episodeLimit)
                delegate: Rectangle {
                    required property var modelData
                    readonly property string primaryTitle: modelData.episodeName || modelData.title || ("Episode " + modelData.number)
                    readonly property string secondaryTitle: modelData.alternativeTitle && modelData.alternativeTitle !== primaryTitle ? modelData.alternativeTitle : ""
                    readonly property var downloadRecord: root.downloadFor(modelData)
                    Layout.fillWidth: true; Layout.preferredHeight: secondaryTitle.length > 0 ? 82 : 72
                    color: episodeHover.hovered ? Theme.raised : Theme.surface; radius: Theme.radius
                    border.width: episodeHover.hovered ? 1 : 0; border.color: Theme.border
                    HoverHandler { id: episodeHover }
                    RowLayout {
                        anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 18
                        Rectangle {
                            Layout.preferredWidth: 46; Layout.preferredHeight: 38; radius: 19; color: "#26E50914"
                            Text { anchors.centerIn: parent; text: modelData.number; color: Theme.red; font.bold: true; font.pixelSize: 13 }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 3
                            Text { text: primaryTitle; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                            Text { visible: secondaryTitle.length > 0; text: secondaryTitle; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; Layout.fillWidth: true }
                        }
                        AppButton {
                            text: "Play"; compact: true
                            onClicked: Player.open(Object.assign({}, modelData, { animeId: root.animeId, animeName: Provider.details.title, animeImage: Provider.details.poster, audioMode: Runtime.audioPreference, server: Runtime.serverPreference }))
                        }
                        AppButton {
                            text: root.downloadLabel(downloadRecord); compact: true
                            secondary: downloadRecord.state !== "completed"
                            enabled: Account.authenticated
                            onClicked: {
                                if (downloadRecord.state === "completed") Player.openOffline(downloadRecord)
                                else if (["paused", "failed", "cancelled"].indexOf(downloadRecord.state) >= 0) Downloads.resume(downloadRecord.id)
                                else if (["preparing", "queued", "downloading", "validating"].indexOf(downloadRecord.state) >= 0) Runtime.route = "downloads"
                                else Downloads.enqueueEpisode(Object.assign({}, modelData, { animeId: root.animeId, animeName: Provider.details.title, animeImage: Provider.details.poster, audioMode: Runtime.audioPreference, server: Runtime.serverPreference }), Runtime.downloadQuality)
                            }
                        }
                    }
                }
            }
            Text { visible: Downloads.error.length > 0; text: Downloads.error; color: Theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            Text {
                visible: root.matchingEpisodes.length > 0
                text: "Showing " + Math.min(root.episodeLimit, root.matchingEpisodes.length) + " of " + root.matchingEpisodes.length
                color: Theme.muted; font.pixelSize: 12; Layout.alignment: Qt.AlignHCenter
            }
            AppButton {
                visible: root.episodeLimit < root.matchingEpisodes.length
                text: "Load 24 more"; secondary: true; Layout.alignment: Qt.AlignHCenter
                onClicked: root.episodeLimit += 24
            }
            EmptyState {
                visible: episodeSearch.text.length > 0 && root.matchingEpisodes.length === 0
                Layout.fillWidth: true; Layout.preferredHeight: 180
                title: "No matching episode"; message: "Try another episode number or title."; symbol: "?"
            }
            PosterRail { title: "You may also like"; model: Provider.recommendations; Layout.fillWidth: true; onActivated: anime => Runtime.route = "details/" + anime.id }
        }
    }
    LoadingSkeleton { anchors.fill: parent; coverPage: true; rows: 4; active: Provider.loading && !Provider.details.title; message: "Loading anime details…"; z: 5 }
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
