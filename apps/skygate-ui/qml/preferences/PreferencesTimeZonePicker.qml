import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.skygate.app 1.0

Item {
    id: root
    readonly property var theme: skyContext.theme
    property var catalogModel
    property string placeholderText: "Choose a timezone"
    property string selectedTimeZoneId: ""
    property string selectedText: ""
    readonly property var hostWindow: Window.window
    readonly property real popupSpacing: 4
    readonly property real popupMargin: 10
    readonly property point popupOriginInWindow: hostWindow
                                              ? mapToItem(hostWindow.contentItem, 0, 0)
                                              : Qt.point(0, 0)
    readonly property real availableSpaceAbove: hostWindow
                                             ? Math.max(0, popupOriginInWindow.y - popupMargin)
                                             : 320
    readonly property real availableSpaceBelow: hostWindow
                                             ? Math.max(
                                                   0,
                                                   hostWindow.contentItem.height
                                                       - (popupOriginInWindow.y + height)
                                                       - popupMargin
                                               )
                                             : 320

    signal timeZoneChosen(string timeZoneId, string displayText)

    implicitHeight: 32

    Rectangle {
        id: buttonBackground
        anchors.fill: parent
        radius: 8
        color: root.enabled ? root.theme.inputBackground : root.theme.inputBackgroundDisabled
        border.width: 1
        border.color: timeZonePopup.opened ? root.theme.inputBorderFocus : root.theme.inputBorder
        opacity: root.enabled ? 1.0 : 0.72

        Text {
            anchors.left: parent.left
            anchors.right: popupIndicator.left
            anchors.leftMargin: 9
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: root.selectedText !== "" ? root.selectedText : root.placeholderText
            color: root.selectedText !== ""
                ? root.theme.inputText
                : root.theme.inputPlaceholderText
            font.family: "Avenir Next"
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        Text {
            id: popupIndicator
            anchors.right: parent.right
            anchors.rightMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            text: "\u25BE"
            color: root.theme.inputIndicator
            font.pixelSize: 8
        }

        MouseArea {
            objectName: "timeZonePickerMouse"
            anchors.fill: parent
            enabled: root.enabled
            hoverEnabled: true
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: timeZonePopup.open()
        }
    }

    Popup {
        id: timeZonePopup
        readonly property real desiredHeight: 320
        readonly property bool openAbove: root.availableSpaceBelow < desiredHeight
                                          && root.availableSpaceAbove > root.availableSpaceBelow
        readonly property real popupHeight: Math.min(
            desiredHeight,
            openAbove ? root.availableSpaceAbove : root.availableSpaceBelow
        )
        x: 0
        y: openAbove ? -popupHeight - root.popupSpacing : root.height + root.popupSpacing
        width: root.width
        height: popupHeight
        padding: 8
        clip: true
        margins: root.popupMargin

        onOpened: {
            searchField.text = ""
            if (root.catalogModel) {
                root.catalogModel.filterText = ""
            }
            searchField.forceActiveFocus()
        }

        onClosed: {
            if (root.catalogModel) {
                root.catalogModel.filterText = ""
            }
        }

        contentItem: ColumnLayout {
            id: popupLayout
            spacing: 8

            PreferencesTextField {
                id: searchField
                objectName: "timeZoneSearchField"
                Layout.fillWidth: true
                placeholderText: "Filter by timezone, label, or offset"
                onTextChanged: {
                    if (root.catalogModel) {
                        root.catalogModel.filterText = text
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(
                    0,
                    Math.min(
                        240,
                        timeZonePopup.availableHeight - searchField.implicitHeight - popupLayout.spacing
                    )
                )

                ListView {
                    id: timeZoneListView
                    objectName: "timeZoneListView"
                    anchors.fill: parent
                    clip: true
                    model: root.catalogModel
                    spacing: 2

                    delegate: Rectangle {
                        required property string displayText
                        required property string detailText
                        required property string timeZoneId
                        required property bool selectable

                        width: timeZoneListView.width
                        height: 42
                        radius: 8
                        color: {
                            if (root.selectedTimeZoneId === timeZoneId) {
                                return root.theme.listItemBackgroundSelected
                            }

                            return delegateMouse.containsMouse
                                ? root.theme.listItemBackgroundHover
                                : "transparent"
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 9
                            anchors.rightMargin: 9
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 1

                            Text {
                                text: displayText
                                color: root.theme.listItemPrimaryText
                                font.family: "Avenir Next"
                                font.pixelSize: 11
                                elide: Text.ElideRight
                            }

                            Text {
                                text: detailText
                                color: root.theme.listItemSecondaryText
                                font.family: "Avenir Next"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: delegateMouse
                            objectName: "timeZoneDelegateMouse_" + timeZoneId
                            anchors.fill: parent
                            enabled: selectable
                            hoverEnabled: selectable
                            cursorShape: selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                root.timeZoneChosen(timeZoneId, displayText + " - " + detailText)
                                timeZonePopup.close()
                            }
                        }
                    }
                }

                Label {
                    objectName: "timeZoneEmptyLabel"
                    anchors.centerIn: parent
                    visible: timeZoneListView.count === 0
                    text: "No matching timezones"
                    color: root.theme.emptyStateText
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }
            }
        }

        background: Rectangle {
            radius: 9
            color: root.theme.cardBackground
            border.width: 1
            border.color: root.theme.toolbarDropdownBorder
        }
    }
}
