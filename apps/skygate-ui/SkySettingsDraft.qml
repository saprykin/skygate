import QtQml

QtObject {
    id: draft
    required property var skyContextController

    property string latitudeText: ""
    property string longitudeText: ""
    property string elevationText: ""
    property string locationSourceText: ""
    property string selectedCityId: ""
    property string selectedCityDisplayText: ""
    property string projectionTypeText: ""
    property string themeId: ""
    property bool overlayHorizon: true
    property bool overlayAltAzGrid: true
    property bool overlayConstellationLines: true
    property bool overlayConstellationLabels: true
    property bool overlayEcliptic: false
    property bool overlayCelestialEquator: false
    property bool overlayCircumpolarBoundary: false
    property bool overlaySolarSystemLabels: true
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
        latitudeText = skyContextController.latitudeText
        longitudeText = skyContextController.longitudeText
        elevationText = skyContextController.elevationText
        locationSourceText = skyContextController.locationSourceText
        selectedCityId = skyContextController.selectedCityId
        selectedCityDisplayText = skyContextController.selectedCityDisplayText
        projectionTypeText = skyContextController.projectionTypeText
        themeId = skyContextController.themeId
        overlayHorizon = skyContextController.overlayLayers.horizon
        overlayAltAzGrid = skyContextController.overlayLayers.altAzGrid
        overlayConstellationLines = skyContextController.overlayLayers.constellationLines
        overlayConstellationLabels = skyContextController.overlayLayers.constellationLabels
        overlayEcliptic = skyContextController.overlayLayers.ecliptic
        overlayCelestialEquator = skyContextController.overlayLayers.celestialEquator
        overlayCircumpolarBoundary = skyContextController.overlayLayers.circumpolarBoundary
        overlaySolarSystemLabels = skyContextController.overlayLayers.solarSystemLabels
        catalogPresetIndex = skyContextController.catalogPresetIndex()
        catalogUrlText = skyContextController.catalogUrlText()
    }

    function syncDeviceLocationFromContext() {
        if (locationSourceText !== "Current Device") {
            return
        }

        latitudeText = skyContextController.latitudeText
        longitudeText = skyContextController.longitudeText
        elevationText = skyContextController.elevationText
    }

    function applyToContext() {
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
        skyContextController.setThemeId(themeId)
        skyContextController.overlayLayers.horizon = overlayHorizon
        skyContextController.overlayLayers.altAzGrid = overlayAltAzGrid
        skyContextController.overlayLayers.constellationLines = overlayConstellationLines
        skyContextController.overlayLayers.constellationLabels = overlayConstellationLabels
        skyContextController.overlayLayers.ecliptic = overlayEcliptic
        skyContextController.overlayLayers.celestialEquator = overlayCelestialEquator
        skyContextController.overlayLayers.circumpolarBoundary = overlayCircumpolarBoundary
        skyContextController.overlayLayers.solarSystemLabels = overlaySolarSystemLabels
        skyContextController.setCatalogPresetIndex(catalogPresetIndex)
        skyContextController.setCatalogUrlText(catalogUrlText)
        resetFromContext()
    }
}
