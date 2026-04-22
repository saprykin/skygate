import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Item {
    id: root
    readonly property var theme: skyContext.theme
    property var catalogModel
    property string placeholderText: "Choose a city"
    property string selectedCityId: ""
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

    signal cityChosen(string cityId, string displayText, double latitudeDeg, double longitudeDeg)

    implicitHeight: 32

    Rectangle {
        id: buttonBackground
        anchors.fill: parent
        radius: 8
        color: root.enabled ? root.theme.inputBackground : root.theme.inputBackgroundDisabled
        border.width: 1
        border.color: cityPopup.opened ? root.theme.inputBorderFocus : root.theme.inputBorder
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
            anchors.fill: parent
            enabled: root.enabled
            hoverEnabled: true
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: cityPopup.open()
        }
    }

    Popup {
        id: cityPopup
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
                Layout.fillWidth: true
                placeholderText: "Filter by city or country"
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
                        cityPopup.availableHeight - searchField.implicitHeight - popupLayout.spacing
                    )
                )

                ListView {
                    id: cityListView
                    anchors.fill: parent
                    clip: true
                    model: root.catalogModel
                    spacing: 2

                    delegate: Rectangle {
                        required property string rowKind
                        required property string displayText
                        required property string detailText
                        required property string cityId
                        required property double latitudeDeg
                        required property double longitudeDeg
                        required property bool selectable

                        width: cityListView.width
                        height: selectable ? 42 : 26
                        radius: selectable ? 8 : 0
                        color: {
                            if (!selectable) {
                                return "transparent"
                            }

                            if (root.selectedCityId === cityId) {
                                return root.theme.listItemBackgroundSelected
                            }

                            return delegateMouse.containsMouse
                                ? root.theme.listItemBackgroundHover
                                : "transparent"
                        }

                        Column {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: selectable ? 9 : 2
                            anchors.rightMargin: 9
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: selectable ? 1 : 0

                            Text {
                                text: displayText
                                color: selectable
                                    ? root.theme.listItemPrimaryText
                                    : root.theme.listItemSectionText
                                font.family: "Avenir Next"
                                font.pixelSize: selectable ? 11 : 10
                                font.weight: selectable ? Font.Normal : Font.DemiBold
                                elide: Text.ElideRight
                            }

                            Text {
                                visible: selectable
                                text: detailText
                                color: root.theme.listItemSecondaryText
                                font.family: "Avenir Next"
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }

                        MouseArea {
                            id: delegateMouse
                            anchors.fill: parent
                            enabled: selectable
                            hoverEnabled: selectable
                            cursorShape: selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
                            onClicked: {
                                root.cityChosen(
                                    cityId,
                                    displayText + ", " + detailText,
                                    latitudeDeg,
                                    longitudeDeg
                                )
                                cityPopup.close()
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: cityListView.count === 0
                    text: "No matching cities"
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
