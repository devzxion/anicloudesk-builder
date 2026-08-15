import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import AniCloud

ApplicationWindow {
    id: window
    width: Math.max(760, Number(Runtime.windowValue("width", 1280)))
    height: Math.max(560, Number(Runtime.windowValue("height", 800)))
    minimumWidth: 720
    minimumHeight: 520
    visible: true
    title: "AniCloud"
    color: Theme.background
    flags: Qt.Window | Qt.FramelessWindowHint
    property bool compactNavigation: width < 900

    Component.onCompleted: {
        const savedX = Number(Runtime.windowValue("x", -1))
        const savedY = Number(Runtime.windowValue("y", -1))
        if (savedX >= 0 && savedY >= 0) { window.x = savedX; window.y = savedY }
    }
    onClosing: close => {
        Player.close()
        Runtime.setWindowValue("x", window.x)
        Runtime.setWindowValue("y", window.y)
        Runtime.setWindowValue("width", window.width)
        Runtime.setWindowValue("height", window.height)
    }

    Rectangle {
        id: titleBar
        anchors.top: parent.top; anchors.left: parent.left; anchors.right: parent.right
        height: 42; color: Theme.background; z: 20
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 14; spacing: 8
            Text { text: "ANICLOUD"; color: Theme.red; font.pixelSize: 15; font.weight: Font.Black }
            Text { text: "NATIVE"; color: Theme.muted; font.pixelSize: 9; Layout.fillWidth: true }
            Button { text: "—"; implicitWidth: 46; implicitHeight: 42; flat: true; palette.buttonText: Theme.text; Accessible.name: "Minimize"; onClicked: window.showMinimized() }
            Button { text: window.visibility === Window.Maximized ? "❐" : "□"; implicitWidth: 46; implicitHeight: 42; flat: true; palette.buttonText: Theme.text; Accessible.name: "Maximize"; onClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized() }
            Button {
                text: "×"; implicitWidth: 46; implicitHeight: 42; flat: true
                palette.buttonText: Theme.text; Accessible.name: "Close"
                background: Rectangle { color: parent.hovered ? Theme.red : "transparent" }
                onClicked: window.close()
            }
        }
        DragHandler { onActiveChanged: if (active) window.startSystemMove() }
        TapHandler { acceptedButtons: Qt.LeftButton; onDoubleTapped: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized() }
    }

    NavigationRail {
        id: navigation
        compact: window.compactNavigation
        currentRoute: Runtime.route
        anchors.top: titleBar.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: compact ? parent.width : 224
        height: compact ? 68 : parent.height - titleBar.height
        anchors.topMargin: compact ? parent.height - titleBar.height - height : 0
        z: 10
        onNavigate: route => Runtime.route = route
    }

    Loader {
        id: pageLoader
        anchors.top: titleBar.bottom
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.left: window.compactNavigation ? parent.left : navigation.right
        anchors.bottomMargin: window.compactNavigation ? navigation.height : 0
        sourceComponent: {
            if (Runtime.route.startsWith("details/")) return detailsPage
            if (Runtime.route === "discover") return discoverPage
            if (Runtime.route === "downloads") return downloadsPage
            if (Runtime.route === "library") return libraryPage
            if (Runtime.route === "profile") return profilePage
            if (Runtime.route === "auth") return authPage
            if (Runtime.route === "broadcasts") return broadcastsPage
            return homePage
        }
        onLoaded: updateDetailsRoute()
        function updateDetailsRoute() {
            if (item && Runtime.route.startsWith("details/"))
                item.animeId = decodeURIComponent(Runtime.route.substring(8))
        }
        Connections { target: Runtime; function onRouteChanged() { pageLoader.updateDetailsRoute() } }
    }

    Component { id: homePage; HomePage {} }
    Component { id: discoverPage; DiscoverPage {} }
    Component { id: detailsPage; DetailsPage {} }
    Component { id: downloadsPage; DownloadsPage {} }
    Component { id: libraryPage; LibraryPage {} }
    Component { id: profilePage; ProfilePage {} }
    Component { id: authPage; AuthPage {} }
    Component { id: broadcastsPage; BroadcastsPage {} }

    PlayerPage {
        id: playerPage
        anchors.fill: parent
        visible: Player.state !== "idle"
        z: 100
        onVisibleChanged: if (visible) forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent; z: 200
        visible: Updates.mandatory
        color: "#F509090B"
        ColumnLayout {
            anchors.centerIn: parent; width: Math.min(520, parent.width - 50); spacing: 16
            Text { text: "AniCloud " + Updates.availableVersion + " is required"; color: Theme.text; font.pixelSize: 28; font.weight: Font.Black; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
            Text { text: "A newer signed desktop release was verified. Update to continue using AniCloud."; color: Theme.muted; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
            ProgressBar { visible: Updates.status === "downloading"; from: 0; to: 1; value: Updates.progress; Layout.fillWidth: true; palette.highlight: Theme.red }
            Text { visible: Updates.error.length > 0; text: Updates.error; color: Theme.red; wrapMode: Text.WordWrap; horizontalAlignment: Text.AlignHCenter; Layout.fillWidth: true }
            AppButton { Layout.alignment: Qt.AlignHCenter; text: Updates.status === "downloading" ? "Downloading…" : Updates.status === "ready" ? "Launch installer" : "Download verified update"; enabled: Updates.status !== "downloading"; onClicked: Updates.status === "ready" ? Updates.launchInstaller() : Updates.downloadAndInstall() }
        }
    }

    Rectangle {
        anchors.fill: parent; z: 190
        visible: (Account.maintenance.enabled || Account.maintenance.active) && !Updates.mandatory
        color: "#F509090B"
        EmptyState { anchors.centerIn: parent; title: Account.maintenance.title || "AniCloud is under maintenance"; message: Account.maintenance.message || "Please try again later."; symbol: "⚙" }
    }

    Shortcut { sequence: "F"; enabled: playerPage.visible; onActivated: window.visibility = window.visibility === Window.FullScreen ? Window.Windowed : Window.FullScreen }
    Connections {
        target: Player
        function onStateChanged() { if (Player.state === "idle" && window.visibility === Window.FullScreen) window.showNormal() }
        function onRequestNextEpisode(current) {
            const number = Number(current.episodeNumber || current.number || 0)
            const next = Provider.episodes.find(item => Number(item.number) === number + 1)
            if (next) Player.open(Object.assign({}, next, { animeId: current.animeId, animeName: current.animeName, animeImage: current.animeImage, audioMode: current.audioMode || Runtime.audioPreference }))
            else Player.close()
        }
    }
    Connections {
        target: Account
        function onBroadcastsChanged() {
            if (Account.broadcasts.length && !Account.broadcasts[0].read) Runtime.showNotification(Account.broadcasts[0].title || "AniCloud", Account.broadcasts[0].message || "New broadcast")
        }
    }
}
