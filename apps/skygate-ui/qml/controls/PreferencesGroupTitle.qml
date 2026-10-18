import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Label {
    id: root
    required property var theme
    property int columnSpan: 1

    Layout.columnSpan: root.columnSpan
    color: root.theme.sectionTitleText
    font.family: "Avenir Next"
    font.pixelSize: 12
    font.weight: Font.DemiBold
}
