import QtQuick
import com.skygate.app 1.0

Item {
    id: root
    required property var theme
    property var catalogModel
    property string placeholderText: "Choose a city"
    property string selectedCityId: ""
    property string selectedText: ""
    readonly property alias hostWindow: pickerShell.hostWindow
    readonly property alias popupSpacing: pickerShell.popupSpacing
    readonly property alias popupMargin: pickerShell.popupMargin
    readonly property alias popupOriginInWindow: pickerShell.popupOriginInWindow
    readonly property alias availableSpaceAbove: pickerShell.availableSpaceAbove
    readonly property alias availableSpaceBelow: pickerShell.availableSpaceBelow

    signal cityChosen(string cityId, string displayText, double latitudeDeg, double longitudeDeg)

    implicitHeight: 32

    PreferencesSearchablePickerShell {
        id: pickerShell
        anchors.fill: parent
        enabled: root.enabled
        theme: root.theme
        catalogModel: root.catalogModel
        placeholderText: root.placeholderText
        selectedText: root.selectedText
        pickerMouseObjectName: "cityPickerMouse"
        searchFieldObjectName: "citySearchField"
        listViewObjectName: "cityListView"
        emptyLabelObjectName: "cityEmptyLabel"
        filterPlaceholderText: "Filter by city or country"
        emptyStateText: "No matching cities"

        rowDelegate: Component {
            Rectangle {
                required property string rowKind
                required property string displayText
                required property string detailText
                required property string cityId
                required property double latitudeDeg
                required property double longitudeDeg
                required property bool selectable

                width: ListView.view.width
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
                    objectName: "cityDelegateMouse_" + cityId
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
                        pickerShell.close()
                    }
                }
            }
        }
    }
}
