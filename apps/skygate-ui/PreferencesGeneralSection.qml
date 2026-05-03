import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Item {
    id: root
    required property var settingsDraft

    function localPathFromFileUrl(fileUrl) {
        const text = fileUrl.toString()
        if (text.startsWith("file://")) {
            return decodeURIComponent(text.substring(7))
        }
        return text
    }

    FileDialog {
        id: logFileDialog
        title: "Choose Log File"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Log files (*.log *.txt)", "All files (*)"]
        onAccepted: root.settingsDraft.logFilePath = root.localPathFromFileUrl(selectedFile)
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        spacing: 14

        GridLayout {
            Layout.fillWidth: true
            columns: 4
            rowSpacing: 7
            columnSpacing: 8

            PreferencesGroupTitle {
                columnSpan: 4
                text: "Logging"
            }

            Label {
                text: "Output"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 3
                spacing: 18

                RowLayout {
                    spacing: 7

                    PreferencesCheckBox {
                        checked: root.settingsDraft.logToTerminal
                        onToggled: root.settingsDraft.logToTerminal = checked
                    }

                    Label {
                        text: "Terminal"
                        color: skyContext.theme.formLabelText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                RowLayout {
                    spacing: 7

                    PreferencesCheckBox {
                        checked: root.settingsDraft.logToFile
                        onToggled: root.settingsDraft.logToFile = checked
                    }

                    Label {
                        text: "File"
                        color: skyContext.theme.formLabelText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            Label {
                text: "Log file"
                color: skyContext.theme.formLabelText
                font.family: "Avenir Next"
                font.pixelSize: 10
                Layout.alignment: Qt.AlignVCenter
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 3
                spacing: 8

                PreferencesTextField {
                    objectName: "logFilePathField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 260
                    text: root.settingsDraft.logFilePath
                    enabled: root.settingsDraft.logToFile
                    placeholderText: "Default app data log path"
                    onEditingFinished: root.settingsDraft.logFilePath = text
                }

                PreferencesActionButton {
                    text: "Browse..."
                    implicitWidth: 88
                    enabled: root.settingsDraft.logToFile
                    onClicked: logFileDialog.open()
                }
            }
        }
    }
}
