import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ToolButton {
    id: appMenuButton
    objectName: "appMenuButton"
    required property var theme
    property bool closingFromMenuItem: false
    property bool suppressTooltip: false
    property real lastPopupClosedAt: 0
    signal aboutRequested()
    signal preferencesRequested()

    width: 30
    height: 44
    implicitWidth: width
    implicitHeight: height
    text: "\u22EE"
    font.pixelSize: 18
    font.weight: Font.DemiBold
    z: 25
    onClicked: {
        suppressTooltip = true
        if (appMenuPopup.opened) {
            appMenuPopup.close()
            return
        }
        if (Date.now() - lastPopupClosedAt < 150) {
            return
        }
        appMenuPopup.open()
    }

    ToolTip.visible: hovered && !suppressTooltip && !appMenuPopup.opened
    ToolTip.delay: 250
    ToolTip.text: "SkyGate menu"

    contentItem: Text {
        text: appMenuButton.text
        color: appMenuButton.theme.toolbarToggleText
        font: appMenuButton.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: 8
        color: appMenuButton.down
            ? appMenuButton.theme.toolbarToggleBackgroundPressed
            : (appMenuButton.hovered
                   ? appMenuButton.theme.toolbarToggleBackgroundHover
                   : appMenuButton.theme.toolbarToggleBackground)
        border.width: 1
        border.color: appMenuButton.theme.toolbarToggleBorder
    }

    Popup {
        id: appMenuPopup
        objectName: "appMenuPopup"
        x: 0
        y: appMenuButton.height + 6
        width: 178
        padding: 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: {
            appMenuButton.lastPopupClosedAt = appMenuButton.closingFromMenuItem ? 0 : Date.now()
            appMenuButton.closingFromMenuItem = false
            appMenuButton.suppressTooltip = false
        }

        contentItem: ColumnLayout {
            spacing: 2

            component AppMenuItem : ItemDelegate {
                id: appMenuItem
                Layout.fillWidth: true
                implicitHeight: 34
                leftPadding: 10
                rightPadding: 10
                topPadding: 6
                bottomPadding: 6

                contentItem: Text {
                    text: appMenuItem.text
                    color: appMenuItem.highlighted || appMenuItem.hovered
                        ? appMenuButton.theme.toolbarPrimaryText
                        : appMenuButton.theme.toolbarSecondaryText
                    font.family: "Avenir Next"
                    font.pixelSize: 12
                    font.weight: Font.DemiBold
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                background: Rectangle {
                    radius: 7
                    color: appMenuItem.down
                        ? appMenuButton.theme.toolbarItemBackgroundSelected
                        : (appMenuItem.hovered
                               ? appMenuButton.theme.toolbarItemBackgroundHover
                               : "transparent")
                }
            }

            AppMenuItem {
                objectName: "appMenuPreferencesItem"
                text: "Preferences"
                onClicked: {
                    appMenuButton.closingFromMenuItem = true
                    appMenuPopup.close()
                    appMenuButton.preferencesRequested()
                }
            }

            AppMenuItem {
                objectName: "appMenuAboutItem"
                text: "About"
                onClicked: {
                    appMenuButton.closingFromMenuItem = true
                    appMenuPopup.close()
                    appMenuButton.aboutRequested()
                }
            }
        }

        background: Rectangle {
            radius: 10
            color: appMenuButton.theme.toolbarDropdownBackground
            border.width: 1
            border.color: appMenuButton.theme.toolbarDropdownBorder
        }
    }
}
