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
    property string projectionTypeText: ""
    property int catalogPresetIndex: 0
    property string catalogUrlText: ""

    function resetFromContext() {
        utcDateLocked = skyContextController.utcDateLocked
        utcTimeLocked = skyContextController.utcTimeLocked
        utcDateText = skyContextController.utcDateText
        utcTimeText = skyContextController.utcTimeText
        latitudeText = skyContextController.latitudeText
        longitudeText = skyContextController.longitudeText
        elevationText = skyContextController.elevationText
        projectionTypeText = skyContextController.projectionTypeText
        catalogPresetIndex = skyContextController.catalogPresetIndex()
        catalogUrlText = skyContextController.catalogUrlText()
    }

    function applyToContext() {
        skyContextController.setUtcDateLocked(utcDateLocked)
        skyContextController.setUtcTimeLocked(utcTimeLocked)
        skyContextController.setUtcDateText(utcDateText)
        skyContextController.setUtcTimeText(utcTimeText)
        skyContextController.setLatitudeText(latitudeText)
        skyContextController.setLongitudeText(longitudeText)
        skyContextController.setElevationText(elevationText)
        skyContextController.setProjectionTypeText(projectionTypeText)
        skyContextController.setCatalogPresetIndex(catalogPresetIndex)
        skyContextController.setCatalogUrlText(catalogUrlText)
        resetFromContext()
    }
}
