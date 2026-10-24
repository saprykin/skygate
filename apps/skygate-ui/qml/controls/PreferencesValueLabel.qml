import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Label {
    id: valueLabel
    required property var theme

    Layout.fillWidth: true
    color: valueLabel.theme.listItemPrimaryText
    font.family: "Avenir Next"
    font.pixelSize: 11
    elide: Text.ElideRight
    verticalAlignment: Text.AlignVCenter
}
