import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import com.skygate.app 1.0

Item {
    id: root
    required property var theme
    property var catalogModel
    property string placeholderText: ""
    property string selectedText: ""
    property Component rowDelegate
    property string pickerMouseObjectName: ""
    property string searchFieldObjectName: ""
    property string listViewObjectName: ""
    property string emptyLabelObjectName: ""
    property string filterPlaceholderText: ""
    property string emptyStateText: ""
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

    implicitHeight: 32

    function close() {
        pickerPopup.close()
    }

    Rectangle {
        id: buttonBackground
        anchors.fill: parent
        radius: 8
        color: root.enabled ? root.theme.inputBackground : root.theme.inputBackgroundDisabled
        border.width: 1
        border.color: pickerPopup.opened ? root.theme.inputBorderFocus : root.theme.inputBorder
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
            objectName: root.pickerMouseObjectName
            anchors.fill: parent
            enabled: root.enabled
            hoverEnabled: true
            cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: pickerPopup.open()
        }
    }

    Popup {
        id: pickerPopup
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
                theme: root.theme
                id: searchField
                objectName: root.searchFieldObjectName
                Layout.fillWidth: true
                placeholderText: root.filterPlaceholderText
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
                        pickerPopup.availableHeight - searchField.implicitHeight - popupLayout.spacing
                    )
                )

                ListView {
                    id: itemListView
                    objectName: root.listViewObjectName
                    anchors.fill: parent
                    clip: true
                    model: root.catalogModel
                    spacing: 2
                    delegate: root.rowDelegate
                }

                Label {
                    objectName: root.emptyLabelObjectName
                    anchors.centerIn: parent
                    visible: itemListView.count === 0
                    text: root.emptyStateText
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
