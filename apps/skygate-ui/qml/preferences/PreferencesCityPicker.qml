import QtQuick
import com.skygate.app 1.0

Item {
    id: root
    required property var theme
    property var catalogModel
    property string placeholderText: "Choose a city"
    property string selectedCityId: ""
    property string selectedText: ""

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
            PreferencesPickerRow {
                theme: root.theme
                timeZoneId: ""
                selected: root.selectedCityId === cityId
                delegateMouseObjectName: "cityDelegateMouse_" + cityId
                onActivated: {
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
