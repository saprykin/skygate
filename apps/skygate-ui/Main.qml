import QtQuick
import QtQuick.Controls
import QtQuick.Window
import com.skygate.app 1.0

ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 1100
    height: 760
    minimumWidth: 560
    minimumHeight: 420
    visible: true
    title: "SkyGate"
    readonly property bool nativeMenuBarVisible: Qt.platform.os === "osx"

    menuBar: MenuBar {
        objectName: "nativeMenuBar"
        visible: root.nativeMenuBarVisible
        height: visible ? implicitHeight : 0

        Menu {
            title: "&App"

            MenuItem {
                objectName: "aboutMenuItem"
                text: "&About SkyGate"
                onTriggered: root.openAboutWindow()
            }

            MenuSeparator {}

            MenuItem {
                objectName: "preferencesMenuItem"
                text: "&Preferences..."
                onTriggered: root.openPreferencesWindow()
            }
        }
    }

    AboutWindow {
        id: aboutWindow
        objectName: "aboutWindow"
        transientParentWindow: root
    }

    PreferencesWindow {
        id: preferencesWindow
        objectName: "preferencesWindow"
        skyContextController: skyContext
        transientParentWindow: root
    }

    footer: StatusFooter {
        id: statusFooter
        skyContextController: skyContext
        sceneModel: skyScene
        dateTimePopupOpen: dateTimePopup.opened
        nightConditionsPopupOpen: nightConditionsPopup.opened
        onDateTimeClicked: {
            nightConditionsPopup.close()
            if (dateTimePopup.opened) {
                dateTimePopup.cancel()
                return
            }

            dateTimePopup.open()
        }
        onNightConditionsClicked: {
            dateTimePopup.close()
            if (nightConditionsPopup.opened) {
                nightConditionsPopup.cancel()
                return
            }

            nightConditionsPopup.open()
        }
    }

    function openAboutWindow() {
        aboutWindow.visible = true
        aboutWindow.raise()
        aboutWindow.requestActivate()
    }

    function openPreferencesWindow() {
        preferencesWindow.openWindow()
    }

    function toolbarItemRight(toolbar, item) {
        return toolbar.x + item.x + item.width
    }

    function toolbarItemLeft(toolbar, item) {
        return toolbar.x + item.x
    }

    function topToolbarVisibleBounds() {
        return {
            searchRight: searchToolbarVisibleRight(),
            timelineLeft: timelineToolbarVisibleLeft()
        }
    }

    function expandedToolbarsWouldOverlap() {
        const bounds = topToolbarVisibleBounds()
        return bounds.searchRight > bounds.timelineLeft
    }

    function searchToolbarExpandedRight() {
        return searchToolbar.x + searchToolbar.expandedTotalWidth
    }

    function searchToolbarVisibleRight() {
        return skyContext.searchToolbarCollapsed
            ? toolbarItemRight(searchToolbar, searchToolbar.toggleItem)
            : Math.max(
                  toolbarItemRight(searchToolbar, searchToolbar.toggleItem),
                  toolbarItemRight(searchToolbar, searchToolbar.panelItem)
              )
    }

    function timelineToolbarExpandedLeft() {
        return toolbarItemLeft(timelineToolbar, timelineToolbar.toggleItem)
            - timelineToolbar.panelItem.expandedWidth
            - 6
    }

    function timelineToolbarVisibleLeft() {
        return skyContext.timelineToolbarCollapsed
            ? toolbarItemLeft(timelineToolbar, timelineToolbar.toggleItem)
            : Math.min(
                  toolbarItemLeft(timelineToolbar, timelineToolbar.toggleItem),
                  toolbarItemLeft(timelineToolbar, timelineToolbar.panelItem)
              )
    }

    function prepareSearchToolbarExpand() {
        if (searchToolbarExpandedRight() > timelineToolbarVisibleLeft()) {
            skyContext.setTimelineToolbarCollapsed(true)
        }
    }

    function prepareTimelineToolbarExpand() {
        if (searchToolbarVisibleRight() > timelineToolbarExpandedLeft()) {
            skyContext.setSearchToolbarCollapsed(true)
        }
    }

    function collapseTimelineIfBothToolbarsOverlap() {
        if (!skyContext.searchToolbarCollapsed
                && !skyContext.timelineToolbarCollapsed
                && expandedToolbarsWouldOverlap()) {
            skyContext.setTimelineToolbarCollapsed(true)
        }
    }

    function closeFooterPopups() {
        dateTimePopup.close()
        nightConditionsPopup.close()
    }

    onWidthChanged: Qt.callLater(collapseTimelineIfBothToolbarsOverlap)
    Component.onCompleted: Qt.callLater(collapseTimelineIfBothToolbarsOverlap)

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: skyContext.theme.skyGradientTop }
            GradientStop { position: 1.0; color: skyContext.theme.skyGradientBottom }
        }

        SkyViewportItem {
            id: skyViewport
            objectName: "skyViewport"
            anchors.fill: parent
            skySceneModel: skyScene
        }

        SkyInteractionLayer {
            id: interactionLayer
            objectName: "skyInteractionLayer"
            viewportItem: skyViewport
            skyContextController: skyContext
            skySceneModel: skyScene
        }

        SkyAppMenuButton {
            id: appMenuButton
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.topMargin: 18
            anchors.leftMargin: 14
            theme: skyContext.theme
            onPreferencesRequested: root.openPreferencesWindow()
            onAboutRequested: root.openAboutWindow()
        }

        SearchToolbar {
            id: searchToolbar
            objectName: "searchToolbar"
            anchors.top: parent.top
            anchors.left: appMenuButton.right
            anchors.topMargin: 14
            anchors.leftMargin: 6
            skyContextController: skyContext
            onRequestExpand: root.prepareSearchToolbarExpand
            onToggleClicked: root.closeFooterPopups()
        }

        TimelineToolbar {
            id: timelineToolbar
            objectName: "timelineToolbar"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            skyContextController: skyContext
            onRequestExpand: root.prepareTimelineToolbarExpand
            onToggleClicked: root.closeFooterPopups()
        }

        SkyOverlayLayer {
            objectName: "skyOverlayLayer"
            anchors.fill: parent
            sceneModel: skyScene
            interactionLayer: interactionLayer
            avoidItems: [
                appMenuButton,
                searchToolbar.panelItem,
                searchToolbar.toggleItem,
                searchToolbar.dropdownItem,
                timelineToolbar.panelItem,
                timelineToolbar.toggleItem
            ]
        }
    }

    StatusDateTimePopup {
        id: dateTimePopup
        skyContextController: skyContext
        popupRightMargin: 8
        popupBottomMargin: 8
        scrimTopMargin: Math.max(
            appMenuButton.y + appMenuButton.height,
            searchToolbar.y + searchToolbar.height,
            timelineToolbar.y + timelineToolbar.height
        ) + 8
    }

    StatusNightConditionsPopup {
        id: nightConditionsPopup
        skyContextController: skyContext
        popupRightMargin: 96
        popupBottomMargin: 8
        scrimTopMargin: dateTimePopup.scrimTopMargin
    }
}
