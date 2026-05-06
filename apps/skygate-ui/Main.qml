import QtQuick
import QtQuick.Controls
import QtQuick.Window
import com.skygate.app 1.0

ApplicationWindow {
    id: root
    objectName: "mainWindow"
    width: 1100
    height: 760
    visible: true
    title: "SkyGate"

    menuBar: MenuBar {
        Menu {
            title: "&App"

            MenuItem {
                objectName: "aboutMenuItem"
                text: "&About SkyGate"
                onTriggered: {
                    aboutWindow.visible = true
                    aboutWindow.raise()
                    aboutWindow.requestActivate()
                }
            }

            MenuSeparator {}

            MenuItem {
                objectName: "preferencesMenuItem"
                text: "&Preferences..."
                onTriggered: preferencesWindow.openWindow()
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

    StatusDateTimePopup {
        id: dateTimePopup
        skyContextController: skyContext
        popupRightMargin: 8
        popupBottomMargin: 8
    }

    StatusNightConditionsPopup {
        id: nightConditionsPopup
        skyContextController: skyContext
        popupRightMargin: 96
        popupBottomMargin: 8
    }

    function topToolbarExpandedBounds() {
        return {
            searchRight: searchToolbar.x + searchToolbar.expandedTotalWidth,
            timelineLeft: timelineToolbar.x
        }
    }

    function expandedToolbarsWouldOverlap() {
        const bounds = topToolbarExpandedBounds()
        return bounds.searchRight > bounds.timelineLeft
    }

    function prepareSearchToolbarExpand() {
        if (!skyContext.timelineToolbarCollapsed && expandedToolbarsWouldOverlap()) {
            skyContext.setTimelineToolbarCollapsed(true)
        }
    }

    function prepareTimelineToolbarExpand() {
        if (!skyContext.searchToolbarCollapsed && expandedToolbarsWouldOverlap()) {
            skyContext.setSearchToolbarCollapsed(true)
        }
    }

    function enforceToolbarFit() {
        if (!skyContext.searchToolbarCollapsed
                && !skyContext.timelineToolbarCollapsed
                && expandedToolbarsWouldOverlap()) {
            skyContext.setTimelineToolbarCollapsed(true)
        }
    }

    onWidthChanged: Qt.callLater(enforceToolbarFit)
    Component.onCompleted: Qt.callLater(enforceToolbarFit)

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

        SearchToolbar {
            id: searchToolbar
            objectName: "searchToolbar"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 14
            skyContextController: skyContext
            onRequestExpand: root.prepareSearchToolbarExpand
        }

        TimelineToolbar {
            id: timelineToolbar
            objectName: "timelineToolbar"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            skyContextController: skyContext
            onRequestExpand: root.prepareTimelineToolbarExpand
        }

        SkyOverlayLayer {
            objectName: "skyOverlayLayer"
            anchors.fill: parent
            sceneModel: skyScene
            interactionLayer: interactionLayer
            avoidItems: [
                searchToolbar.panelItem,
                searchToolbar.toggleItem,
                searchToolbar.dropdownItem,
                timelineToolbar.panelItem,
                timelineToolbar.toggleItem
            ]
        }
    }
}
