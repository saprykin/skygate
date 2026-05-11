import QtQuick

Item {
    id: searchSelectionMarker
    objectName: "searchSelectionMarker"
    required property var theme
    required property var markerData

    visible: markerData && markerData.kind === "searchSelection"
    x: (markerData && markerData.x !== undefined)
        ? markerData.x - (width * 0.5)
        : 0
    y: (markerData && markerData.y !== undefined)
        ? markerData.y - (height * 0.5)
        : 0
    width: 34
    height: 34
    z: 12

    Rectangle {
        anchors.centerIn: parent
        width: parent.width
        height: parent.height
        radius: width * 0.5
        color: searchSelectionMarker.theme.selectionMarkerFill
        border.width: 3
        border.color: searchSelectionMarker.theme.selectionMarkerBorder
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width + 10
        height: parent.height + 10
        radius: width * 0.5
        color: "transparent"
        border.width: 2
        border.color: searchSelectionMarker.theme.selectionMarkerOuterBorder
    }

    Rectangle {
        anchors.centerIn: parent
        width: 10
        height: 10
        radius: 5
        color: searchSelectionMarker.theme.selectionMarkerCenter
    }
}
