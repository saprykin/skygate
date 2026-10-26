import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Label {
    id: fieldLabel
    required property var theme
    property int textPixelSize: 11

    color: fieldLabel.theme.formLabelText
    font.family: "Avenir Next"
    font.pixelSize: fieldLabel.textPixelSize
    Layout.alignment: Qt.AlignVCenter
}
