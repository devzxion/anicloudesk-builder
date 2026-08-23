import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCore
import AniCloud

Item {
    id: root
    Settings { id: searchSettings; category: "discover"; property string recent: "[]" }
    property int page: 1
    property int categoryIndex: 0
    property var taxonomyItem: null
    property var recentSearches: {
        try { return JSON.parse(searchSettings.recent) } catch (error) { return [] }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 30; spacing: 18
        Text { text: "Discover"; color: Theme.text; font.pixelSize: Theme.titleSize; font.weight: Font.Black }
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            TextField {
                id: searchField; Layout.fillWidth: true; implicitHeight: 46
                placeholderText: "Search anime by title"; color: Theme.text; placeholderTextColor: Theme.muted
                Accessible.name: "Search anime"
                background: Rectangle { color: Theme.surface; radius: Theme.radius; border.color: searchField.activeFocus ? Theme.red : Theme.border }
                onAccepted: root.submitSearch()
            }
            AppButton { text: "Search"; onClicked: root.submitSearch() }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8; visible: root.recentSearches.length > 0 && searchField.text.length === 0 && root.taxonomyItem === null
            Repeater { model: root.recentSearches; AppButton { required property string modelData; text: modelData; compact: true; secondary: true; onClicked: { searchField.text = modelData; root.submitSearch() } } }
        }
        ColumnLayout {
            visible: searchField.text.length === 0; Layout.fillWidth: true; spacing: 8
            Text { text: "Genres"; color: Theme.text; font.pixelSize: 17; font.bold: true }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: 40; orientation: ListView.Horizontal
                spacing: 8; clip: true; model: Provider.genres; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: AppButton {
                    required property var modelData
                    text: modelData.label; compact: true
                    secondary: !root.taxonomyItem || root.taxonomyItem.id !== modelData.id
                    onClicked: root.submitTaxonomy(modelData)
                }
            }
            Text { text: "Themes"; color: Theme.text; font.pixelSize: 17; font.bold: true; Layout.topMargin: 4 }
            ListView {
                Layout.fillWidth: true; Layout.preferredHeight: 40; orientation: ListView.Horizontal
                spacing: 8; clip: true; model: Provider.themes; boundsBehavior: Flickable.StopAtBounds
                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: AppButton {
                    required property var modelData
                    text: modelData.label; compact: true
                    secondary: !root.taxonomyItem || root.taxonomyItem.id !== modelData.id
                    onClicked: root.submitTaxonomy(modelData)
                }
            }
        }
        RowLayout {
            visible: searchField.text.length === 0
            Text { text: root.taxonomyItem ? "Browsing " + root.taxonomyItem.label : "Browse"; color: root.taxonomyItem ? Theme.text : Theme.muted; font.bold: root.taxonomyItem !== null }
            AppButton { visible: root.taxonomyItem !== null; text: "Clear filter"; compact: true; secondary: true; onClicked: { root.taxonomyItem = null; root.page = 1; Provider.loadCategory(root.categoryIndex === 0 ? "recent" : root.categoryIndex === 1 ? "popular" : "airing", 1) } }
            ComboBox {
                visible: root.taxonomyItem === null
                model: ["New Releases", "Most Popular", "Top Airing"]
                currentIndex: root.categoryIndex
                onActivated: {
                    root.categoryIndex = currentIndex
                    root.taxonomyItem = null
                    Provider.loadCategory(currentIndex === 0 ? "recent" : currentIndex === 1 ? "popular" : "airing", 1)
                }
                Accessible.name: "Discover category"
            }
        }
        Text { visible: Provider.error.length > 0; text: Provider.error; color: Theme.red; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        GridView {
            id: grid
            Layout.fillWidth: true; Layout.fillHeight: true; clip: true
            cellWidth: 172; cellHeight: 278
            model: Provider.searchResults.length > 0 || searchField.text.length > 0 || root.taxonomyItem !== null ? Provider.searchResults : root.categoryIndex === 0 ? Provider.recent : root.categoryIndex === 1 ? Provider.popular : Provider.airing
            ScrollBar.vertical: ScrollBar {}
            delegate: AnimeCard { anime: modelData; onActivated: value => Runtime.route = "details/" + value.id }
            onAtYEndChanged: if (atYEnd && Provider.searchHasMore && !Provider.loading) {
                root.page++
                if (root.taxonomyItem) Provider.browseGenre(root.taxonomyItem.id, root.taxonomyItem.slug, root.page)
                else if (searchField.text.length > 0) Provider.search(searchField.text, root.page)
            }
        }
        EmptyState { visible: !Provider.loading && grid.count === 0; title: "No anime found"; message: "Try another title or spelling."; Layout.fillWidth: true; Layout.fillHeight: true }
    }

    LoadingSkeleton {
        anchors.fill: parent; coverPage: true; rows: 4; message: "Finding anime…"; z: 5
        active: Provider.loading && grid.count === 0
    }

    function submitSearch() {
        const query = searchField.text.trim()
        if (!query.length) return
        taxonomyItem = null
        page = 1
        let values = recentSearches.filter(value => value.toLowerCase() !== query.toLowerCase())
        values.unshift(query)
        recentSearches = values.slice(0, 8)
        searchSettings.recent = JSON.stringify(recentSearches)
        Provider.search(query, 1)
    }

    function submitTaxonomy(item) {
        taxonomyItem = item
        searchField.text = ""
        page = 1
        Provider.browseGenre(item.id, item.slug, 1)
    }

    Component.onCompleted: Provider.loadTaxonomy()
}
