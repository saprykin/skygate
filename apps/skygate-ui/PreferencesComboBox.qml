import QtQuick
import QtQuick.Controls

ComboBox {
    id: comboControl
    font.family: "Avenir Next"
    font.pixelSize: 11
    implicitHeight: 32
    leftPadding: 9
    rightPadding: 26
    topPadding: 4
    bottomPadding: 4

    contentItem: Text {
        text: comboControl.displayText
        color: "#f1f3ff"
        font: comboControl.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        text: "\u25BE"
        color: "#9ca3c5"
        font.pixelSize: 8
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 9
    }

    background: Rectangle {
        radius: 8
        color: comboControl.enabled ? "#232742" : "#1b1e33"
        border.width: 1
        border.color: comboControl.activeFocus ? "#628fd9" : "#4d5378"
    }

    popup: Popup {
        y: comboControl.height + 4
        width: comboControl.width
        padding: 4
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 220)

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: comboControl.delegateModel
            currentIndex: comboControl.highlightedIndex
        }

        background: Rectangle {
            radius: 9
            color: "#1d2138"
            border.width: 1
            border.color: "#4b5278"
        }
    }

    delegate: ItemDelegate {
        id: preferencesComboDelegate
        width: comboControl.width - 8
        highlighted: comboControl.highlightedIndex === index

        contentItem: Text {
            text: modelData
            color: highlighted ? "#f2f4ff" : "#c1c7e4"
            font: comboControl.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: highlighted ? "#365e9c" : (preferencesComboDelegate.hovered ? "#272c46" : "transparent")
        }
    }
}
