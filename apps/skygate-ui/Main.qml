import QtQuick
import QtQuick.Controls
import QtQuick.Window
import com.skygate.app 1.0

ApplicationWindow {
    id: root
    width: 1100
    height: 760
    visible: true
    title: "SkyGate"

    menuBar: MenuBar {
        Menu {
            title: "&App"

            MenuItem {
                text: "&About SkyGate"
                onTriggered: {
                    aboutWindow.visible = true
                    aboutWindow.raise()
                    aboutWindow.requestActivate()
                }
            }

            MenuSeparator {}

            MenuItem {
                text: "&Preferences..."
                onTriggered: preferencesWindow.openWindow()
            }
        }
    }

    AboutWindow {
        id: aboutWindow
        transientParentWindow: root
    }

    PreferencesWindow {
        id: preferencesWindow
        skyContextController: skyContext
        transientParentWindow: root
    }

    footer: StatusFooter {
        id: statusFooter
        skyContextController: skyContext
        dateTimePopupOpen: dateTimePopup.opened
        onDateTimeClicked: dateTimePopup.open()
    }

    StatusDateTimePopup {
        id: dateTimePopup
        skyContextController: skyContext
        popupRightMargin: 8
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

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#081022" }
            GradientStop { position: 1.0; color: "#040912" }
        }

        SkyViewportItem {
            id: skyViewport
            anchors.fill: parent
            skySceneModel: skyScene
        }

        SkyInteractionLayer {
            id: interactionLayer
            viewportItem: skyViewport
            skyContextController: skyContext
            skySceneModel: skyScene
        }

        SearchToolbar {
            id: searchToolbar
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 14
            skyContextController: skyContext
            onRequestExpand: root.prepareSearchToolbarExpand
        }

        TimelineToolbar {
            id: timelineToolbar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            skyContextController: skyContext
            onRequestExpand: root.prepareTimelineToolbarExpand
        }

        SkyOverlayLayer {
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
