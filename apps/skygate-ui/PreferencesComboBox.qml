import QtQuick
import QtQuick.Controls

ComboBox {
    id: comboControl
    readonly property var theme: skyContext.theme
    font.family: "Avenir Next"
    font.pixelSize: 11
    implicitHeight: 32
    leftPadding: 9
    rightPadding: 26
    topPadding: 4
    bottomPadding: 4

    contentItem: Text {
        text: comboControl.displayText
        color: comboControl.theme.inputText
        font: comboControl.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        text: "\u25BE"
        color: comboControl.theme.inputIndicator
        font.pixelSize: 8
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 9
    }

    background: Rectangle {
        radius: 8
        color: comboControl.enabled
            ? comboControl.theme.inputBackground
            : comboControl.theme.inputBackgroundDisabled
        border.width: 1
        border.color: comboControl.activeFocus
            ? comboControl.theme.inputBorderFocus
            : comboControl.theme.inputBorder
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
            color: comboControl.theme.cardBackground
            border.width: 1
            border.color: comboControl.theme.toolbarDropdownBorder
        }
    }

    delegate: ItemDelegate {
        id: preferencesComboDelegate
        width: comboControl.width - 8
        highlighted: comboControl.highlightedIndex === index

        contentItem: Text {
            text: modelData
            color: highlighted
                ? comboControl.theme.textPrimary
                : comboControl.theme.listItemPrimaryText
            font: comboControl.font
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: 8
            color: highlighted
                ? comboControl.theme.listItemBackgroundSelected
                : (preferencesComboDelegate.hovered
                       ? comboControl.theme.listItemBackgroundHover
                       : "transparent")
        }
    }
}
