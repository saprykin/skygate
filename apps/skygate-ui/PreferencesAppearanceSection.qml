import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var settingsDraft
    readonly property var themeOptions: settingsDraft.skyContextController.themeOptions

    function themeIndexForId(themeId) {
        for (let index = 0; index < themeOptions.length; ++index) {
            if (themeOptions[index].id === themeId) {
                return index
            }
        }
        return 0
    }

    function themeLabels() {
        const labels = []
        for (let index = 0; index < themeOptions.length; ++index) {
            labels.push(themeOptions[index].label)
        }
        return labels
    }

    GridLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        columns: 4
        rowSpacing: 3
        columnSpacing: 6

        Label {
            text: "Theme"
            color: skyContext.theme.formLabelText
            font.family: "Avenir Next"
            font.pixelSize: 10
            Layout.alignment: Qt.AlignVCenter
        }

        PreferencesComboBox {
            id: themeCombo
            Layout.fillWidth: true
            Layout.columnSpan: 3
            model: root.themeLabels()

            Binding on currentIndex {
                value: root.themeIndexForId(root.settingsDraft.themeId)
            }

            onActivated: {
                if (currentIndex >= 0 && currentIndex < root.themeOptions.length) {
                    root.settingsDraft.themeId = root.themeOptions[currentIndex].id
                }
            }
        }
    }
}
