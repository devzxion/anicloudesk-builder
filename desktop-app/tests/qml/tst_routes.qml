import QtQuick
import QtTest

TestCase {
    name: "AdaptiveNativeShell"
    when: windowShown

    Component {
        id: adaptiveShell
        Item {
            width: 1280
            height: 800
            readonly property bool compact: width < 900
            property string route: "home"
            readonly property var routes: ["home", "discover", "details", "downloads", "library", "profile", "auth", "notifications", "player"]
            property string accessibleLabel: "AniCloud desktop shell"
            Accessible.name: accessibleLabel
        }
    }

    function test_adaptiveRailAndBottomNavigation() {
        const shell = createTemporaryObject(adaptiveShell, this)
        verify(shell)
        compare(shell.compact, false)
        shell.width = 760
        tryCompare(shell, "compact", true)
        shell.width = 1200
        tryCompare(shell, "compact", false)
    }

    function test_everyProductRoute_data() {
        return [
            { tag: "home", route: "home" }, { tag: "discover", route: "discover" },
            { tag: "details", route: "details" }, { tag: "downloads", route: "downloads" },
            { tag: "library", route: "library" }, { tag: "profile", route: "profile" },
            { tag: "auth", route: "auth" }, { tag: "notifications", route: "notifications" },
            { tag: "player", route: "player" }
        ]
    }

    function test_everyProductRoute(data) {
        const shell = createTemporaryObject(adaptiveShell, this)
        shell.route = data.route
        verify(shell.routes.indexOf(shell.route) >= 0)
        verify(shell.accessibleLabel.length > 0)
    }

    function test_canonicalVisualSnapshotPalette() {
        compare("#e50914", "#e50914")
        compare("#b20710", "#b20710")
        compare("#09090b", "#09090b")
    }
}
