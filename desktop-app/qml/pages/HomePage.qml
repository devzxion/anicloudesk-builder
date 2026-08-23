import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

Flickable {
    id: root
    contentWidth: width
    contentHeight: content.implicitHeight + 50
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    ScrollBar.vertical: ScrollBar {}
    property int heroIndex: 0
    readonly property var hero: Provider.spotlight.length > 0 ? Provider.spotlight.slice(0, 10) : Provider.popular.slice(0, 10)

    Timer { interval: 5000; repeat: true; running: root.visible && root.hero.length > 1; onTriggered: root.heroIndex = (root.heroIndex + 1) % root.hero.length }

    ColumnLayout {
        id: content
        width: root.width
        spacing: 28

        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: Math.max(370, root.height * 0.58)
            color: Theme.surface; clip: true
            Image {
                anchors.fill: parent
                source: root.hero.length ? (root.hero[root.heroIndex].banner || root.hero[root.heroIndex].poster || "") : ""
                fillMode: Image.PreserveAspectCrop; asynchronous: true
            }
            Rectangle { anchors.fill: parent; gradient: Gradient { GradientStop { position: 0.25; color: "#1009090B" } GradientStop { position: 1; color: Theme.background } } }
            ColumnLayout {
                anchors.left: parent.left; anchors.bottom: parent.bottom; anchors.leftMargin: 42; anchors.bottomMargin: 46
                width: Math.min(parent.width * 0.58, 650); spacing: 12
                Text { text: root.hero.length ? root.hero[root.heroIndex].title : "AniCloud"; color: Theme.text; font.pixelSize: 40; font.weight: Font.Black; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: root.hero.length ? (root.hero[root.heroIndex].synopsis || root.hero[root.heroIndex].description || "Stream anime in a true native desktop experience.") : "Loading the latest anime…"; color: "#D9F8F8FA"; font.pixelSize: 15; maximumLineCount: 3; elide: Text.ElideRight; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                RowLayout {
                    AppButton { text: "View details"; enabled: root.hero.length > 0; onClicked: Runtime.route = "details/" + root.hero[root.heroIndex].id }
                    AppButton { text: "+ My List"; secondary: true; enabled: Account.authenticated && root.hero.length > 0; onClicked: Account.addToWatchlist(root.hero[root.heroIndex]) }
                }
            }
            Row {
                anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 28; spacing: 7
                Repeater { model: root.hero.length; Rectangle { width: index === root.heroIndex ? 22 : 7; height: 7; radius: 4; color: index === root.heroIndex ? Theme.red : Theme.muted; MouseArea { anchors.fill: parent; onClicked: root.heroIndex = index } } }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true; Layout.leftMargin: 32; Layout.rightMargin: 32; spacing: 28
            PosterRail {
                title: "Continue Watching"
                visible: model.length > 0
                model: (Account.authenticated ? Account.history : Runtime.localHistory).slice(0, 6)
                onActivated: anime => Player.open(anime, Number(anime.positionSeconds || anime.timestamp || 0) * 1000)
            }
            PosterRail { title: "New Releases"; model: Provider.recent; onActivated: anime => Runtime.route = "details/" + anime.id }
            PosterRail { title: "Most Popular"; model: Provider.popular; onActivated: anime => Runtime.route = "details/" + anime.id }
            PosterRail { title: "Top Airing"; model: Provider.airing; onActivated: anime => Runtime.route = "details/" + anime.id }
        }
    }

    LoadingSkeleton { anchors.centerIn: parent; width: Math.min(760, parent.width - 64); rows: 4; active: Provider.loading && Provider.popular.length === 0; z: 5 }
    EmptyState { anchors.centerIn: parent; visible: !Provider.loading && root.hero.length === 0 && Provider.error.length > 0; title: "Home is unavailable"; message: Provider.error; symbol: "!" }
}
