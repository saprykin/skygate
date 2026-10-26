import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: root
    required property var preferencesDraft
    required property var theme

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
        onAccepted: root.preferencesDraft.logFilePath = root.localPathFromFileUrl(selectedFile)
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
                theme: root.theme
                columnSpan: 4
                text: "Logging"
            }

            PreferencesFieldLabel {
                theme: root.theme
                text: "Output"
                textPixelSize: 10
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 3
                spacing: 18

                RowLayout {
                    spacing: 7

                    PreferencesCheckBox {
                        theme: root.theme
                        objectName: "logToTerminalCheckBox"
                        checked: root.preferencesDraft.logToTerminal
                        onToggled: root.preferencesDraft.logToTerminal = checked
                    }

                    PreferencesFieldLabel {
                        theme: root.theme
                        text: "Terminal"
                    }
                }

                RowLayout {
                    spacing: 7

                    PreferencesCheckBox {
                        theme: root.theme
                        objectName: "logToFileCheckBox"
                        checked: root.preferencesDraft.logToFile
                        onToggled: root.preferencesDraft.logToFile = checked
                    }

                    PreferencesFieldLabel {
                        theme: root.theme
                        text: "File"
                    }
                }

                Item {
                    Layout.fillWidth: true
                }
            }

            PreferencesFieldLabel {
                theme: root.theme
                text: "Log file"
                textPixelSize: 10
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.columnSpan: 3
                spacing: 8

                PreferencesTextField {
                    theme: root.theme
                    objectName: "logFilePathField"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 260
                    text: root.preferencesDraft.logFilePath
                    enabled: root.preferencesDraft.logToFile
                    placeholderText: "Default app data log path"
                    onEditingFinished: root.preferencesDraft.logFilePath = text
                }

                PreferencesActionButton {
                    theme: root.theme
                    objectName: "logFileBrowseButton"
                    text: "Browse..."
                    implicitWidth: 88
                    enabled: root.preferencesDraft.logToFile
                    onClicked: logFileDialog.open()
                }
            }
        }
    }
}
