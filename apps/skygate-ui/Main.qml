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
        skyContextController: skyContext
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

        TimelineToolbar {
            id: timelineToolbar
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: 14
            skyContextController: skyContext
        }

        SkyOverlayLayer {
            anchors.fill: parent
            sceneModel: skyScene
            interactionLayer: interactionLayer
            toolbarPanel: timelineToolbar.panelItem
            toolbarToggle: timelineToolbar.toggleItem
        }
    }
}
