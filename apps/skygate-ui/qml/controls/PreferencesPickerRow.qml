import QtQuick

// Shared row delegate for the searchable catalog pickers. Qt only injects
// required properties from model roles, so this delegate declares the roles of
// both catalog models it serves; a picker whose model lacks a role supplies it
// explicitly.
Rectangle {
    id: pickerRow
    required property var theme
    required property string displayText
    required property string detailText
    required property bool selectable
    required property string cityId
    required property double latitudeDeg
    required property double longitudeDeg
    required property string timeZoneId
    property bool selected: false
    property string delegateMouseObjectName: ""

    signal activated()

    width: ListView.view.width
    height: selectable ? 42 : 26
    radius: selectable ? 8 : 0
    color: {
        if (!pickerRow.selectable) {
            return "transparent"
        }

        if (pickerRow.selected) {
            return pickerRow.theme.listItemBackgroundSelected
        }

        return delegateMouse.containsMouse
            ? pickerRow.theme.listItemBackgroundHover
            : "transparent"
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: pickerRow.selectable ? 9 : 2
        anchors.rightMargin: 9
        anchors.verticalCenter: parent.verticalCenter
        spacing: pickerRow.selectable ? 1 : 0

        Text {
            text: pickerRow.displayText
            color: pickerRow.selectable
                ? pickerRow.theme.listItemPrimaryText
                : pickerRow.theme.listItemSectionText
            font.family: "Avenir Next"
            font.pixelSize: pickerRow.selectable ? 11 : 10
            font.weight: pickerRow.selectable ? Font.Normal : Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            visible: pickerRow.selectable
            text: pickerRow.detailText
            color: pickerRow.theme.listItemSecondaryText
            font.family: "Avenir Next"
            font.pixelSize: 10
            elide: Text.ElideRight
        }
    }

    MouseArea {
        id: delegateMouse
        objectName: pickerRow.delegateMouseObjectName
        anchors.fill: parent
        enabled: pickerRow.selectable
        hoverEnabled: pickerRow.selectable
        cursorShape: pickerRow.selectable ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: pickerRow.activated()
    }
}
