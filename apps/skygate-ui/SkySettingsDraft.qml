import QtQml

QtObject {
    id: draft
    required property var skyContextController

    property bool utcDateLocked: true
    property bool utcTimeLocked: true
    property string utcDateText: ""
    property string utcTimeText: ""
    property string latitudeText: ""
    property string longitudeText: ""
    property string elevationText: ""
    property string locationSourceText: ""
    property string selectedCityId: ""
    property string selectedCityDisplayText: ""
    property string projectionTypeText: ""
    property int catalogPresetIndex: 0
    property string catalogUrlText: ""

    function setLocationSource(nextLocationSourceText) {
        locationSourceText = nextLocationSourceText
        if (locationSourceText !== "City") {
            selectedCityId = ""
            selectedCityDisplayText = ""
        }
    }

    function selectCity(cityId, displayText, latitudeDeg, longitudeDeg) {
        locationSourceText = "City"
        selectedCityId = cityId
        selectedCityDisplayText = displayText
        latitudeText = Number(latitudeDeg).toFixed(6)
        longitudeText = Number(longitudeDeg).toFixed(6)
    }

    function useCustomCoordinates() {
        if (locationSourceText !== "Custom") {
            locationSourceText = "Custom"
        }
        selectedCityId = ""
        selectedCityDisplayText = ""
    }

    function resetFromContext() {
        utcDateLocked = skyContextController.utcDateLocked
        utcTimeLocked = skyContextController.utcTimeLocked
        utcDateText = skyContextController.utcDateText
        utcTimeText = skyContextController.utcTimeText
        latitudeText = skyContextController.latitudeText
        longitudeText = skyContextController.longitudeText
        elevationText = skyContextController.elevationText
        locationSourceText = skyContextController.locationSourceText
        selectedCityId = skyContextController.selectedCityId
        selectedCityDisplayText = skyContextController.selectedCityDisplayText
        projectionTypeText = skyContextController.projectionTypeText
        catalogPresetIndex = skyContextController.catalogPresetIndex()
        catalogUrlText = skyContextController.catalogUrlText()
    }

    function applyToContext() {
        skyContextController.setUtcDateLocked(utcDateLocked)
        skyContextController.setUtcTimeLocked(utcTimeLocked)
        skyContextController.setUtcDateText(utcDateText)
        skyContextController.setUtcTimeText(utcTimeText)

        if (locationSourceText === "City" && selectedCityId !== "") {
            skyContextController.setLocationSourceText(locationSourceText)
            skyContextController.setSelectedCityId(selectedCityId)
            skyContextController.setElevationText(elevationText)
        } else if (locationSourceText === "Current Device") {
            skyContextController.setLocationSourceText(locationSourceText)
            skyContextController.setElevationText(elevationText)
            skyContextController.refreshCurrentLocation()
        } else {
            skyContextController.setLocationSourceText("Custom")
            skyContextController.setLatitudeText(latitudeText)
            skyContextController.setLongitudeText(longitudeText)
            skyContextController.setElevationText(elevationText)
        }

        skyContextController.setProjectionTypeText(projectionTypeText)
        skyContextController.setCatalogPresetIndex(catalogPresetIndex)
        skyContextController.setCatalogUrlText(catalogUrlText)
        resetFromContext()
    }
}
