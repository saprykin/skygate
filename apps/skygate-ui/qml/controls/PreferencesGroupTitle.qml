import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Label {
    id: root
    property int columnSpan: 1

    Layout.columnSpan: root.columnSpan
    color: skyContext.theme.sectionTitleText
    font.family: "Avenir Next"
    font.pixelSize: 12
    font.weight: Font.DemiBold
}
