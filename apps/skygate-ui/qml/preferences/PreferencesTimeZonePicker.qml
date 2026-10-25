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
            PreferencesPickerRow {
                theme: root.theme
                cityId: ""
                latitudeDeg: 0
                longitudeDeg: 0
                selected: root.selectedTimeZoneId === timeZoneId
                delegateMouseObjectName: "timeZoneDelegateMouse_" + timeZoneId
                onActivated: {
                    root.timeZoneChosen(timeZoneId, displayText + " - " + detailText)
                    pickerShell.close()
                }
            }
        }
    }
}
