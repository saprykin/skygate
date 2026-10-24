import QtQuick
import com.skygate.app 1.0

Item {
    id: root
    required property var theme
    property var catalogModel
    property string placeholderText: "Choose a timezone"
    property string selectedTimeZoneId: ""
    property string selectedText: ""

    signal timeZoneChosen(string timeZoneId, string displayText)

    implicitHeight: 32

    PreferencesSearchablePickerShell {
        id: pickerShell
        anchors.fill: parent
        enabled: root.enabled
        theme: root.theme
        catalogModel: root.catalogModel
        placeholderText: root.placeholderText
        selectedText: root.selectedText
        pickerMouseObjectName: "timeZonePickerMouse"
        searchFieldObjectName: "timeZoneSearchField"
        listViewObjectName: "timeZoneListView"
        emptyLabelObjectName: "timeZoneEmptyLabel"
        filterPlaceholderText: "Filter by timezone, label, or offset"
        emptyStateText: "No matching timezones"

        rowDelegate: Component {
            Rectangle {
                required property string displayText
                required property string detailText
                required property string timeZoneId
                required property bool selectable

                width: ListView.view.width
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
                        pickerShell.close()
                    }
                }
            }
        }
    }
}
