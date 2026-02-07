import QtQuick
import QtQuick.Controls

ComboBox {
    id: comboControl
    font.family: "Avenir Next"
    implicitHeight: 36
    leftPadding: 10
    rightPadding: 30
    topPadding: 5
    bottomPadding: 5

    contentItem: Text {
        text: comboControl.displayText
        color: "#ecf4ff"
        font: comboControl.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        text: "\u25BE"
        color: "#a4c5eb"
        font.pixelSize: 12
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 10
    }

    background: Rectangle {
        radius: 9
        color: "#102544"
        border.width: 1
        border.color: comboControl.activeFocus ? "#8fd9ff" : "#3f648d"
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
            radius: 10
            color: "#112949"
            border.width: 1
            border.color: "#4e79a8"
        }
    }

    delegate: ItemDelegate {
        id: preferencesComboDelegate
        width: comboControl.width - 8
        highlighted: comboControl.highlightedIndex === index

        contentItem: Text {
            text: modelData
            color: highlighted ? "#ecf8ff" : "#bfd6f5"
            font: comboControl.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: highlighted ? "#2f5f94" : (preferencesComboDelegate.hovered ? "#1e3d60" : "transparent")
        }
    }
}
