import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: catalogSection
    required property var skyContextController
    readonly property var theme: skyContextController.theme
    readonly property bool catalogBusy: skyContextController.downloadingCatalog
                                        || skyContextController.catalogProcessing

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            ColumnLayout {
                width: catalogSection.width
                spacing: 6

                PreferencesGroupTitle {
                    theme: catalogSection.theme
                    text: "Active Catalog Sources"
                }

                Label {
                    objectName: "catalogParticipationSummaryLabel"
                    Layout.fillWidth: true
                    text: skyContextController.catalogParticipationSummary
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    Layout.fillWidth: true
                    text: skyContextController.catalogDatasetInfoText
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Repeater {
                    id: sourceRepeater
                    model: skyContextController.catalogSourceCollectionModel

                    delegate: Rectangle {
                        required property string instanceId
                        required property string title
                        required property string version
                        required property string category
                        required property bool sourceEnabled
                        required property bool bundled
                        required property bool busy
                        required property bool hasError
                        required property string status
                        required property int objectCount
                        required property int index

                        objectName: "catalogSourceRow_" + index
                        Layout.fillWidth: true
                        Layout.preferredHeight: sourceRowColumn.implicitHeight + 16
                        radius: 6
                        color: theme.cardBackground
                        border.width: 1
                        border.color: theme.cardBorder

                        ColumnLayout {
                            id: sourceRowColumn
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 5

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 6

                                PreferencesCheckBox {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceEnableCheckBox_" + index
                                    visible: !bundled
                                    checked: sourceEnabled
                                    enabled: !catalogBusy
                                    onClicked: {
                                        if (checked) {
                                            skyContextController.enableCatalogSource(instanceId)
                                        } else {
                                            skyContextController.disableCatalogSource(instanceId)
                                        }
                                    }
                                }

                                Label {
                                    Layout.fillWidth: true
                                    text: version.length > 0 ? title + " " + version : title
                                    color: theme.listItemPrimaryText
                                    font.family: "Avenir Next"
                                    font.pixelSize: 11
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: category
                                    color: theme.textMuted
                                    font.family: "Avenir Next"
                                    font.pixelSize: 9
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                text: objectCount > 0 ? status + " | " + objectCount + " objects" : status
                                color: hasError ? theme.errorText : theme.listItemPrimaryText
                                font.family: "Avenir Next"
                                font.pixelSize: 9
                                elide: Text.ElideRight
                            }

                            Flow {
                                Layout.fillWidth: true
                                spacing: 4

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceUpButton_" + index
                                    text: "Up"
                                    enabled: !catalogBusy && index > 0
                                    onClicked: skyContextController.moveCatalogSource(instanceId, index - 1)
                                }

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceDownButton_" + index
                                    text: "Down"
                                    enabled: !catalogBusy && index < sourceRepeater.count - 1
                                    onClicked: skyContextController.moveCatalogSource(instanceId, index + 1)
                                }

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceRetryButton_" + index
                                    text: "Retry"
                                    visible: hasError
                                    enabled: !catalogBusy
                                    onClicked: skyContextController.retryCatalogSource(instanceId)
                                }

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceCancelButton_" + index
                                    text: "Cancel"
                                    visible: busy
                                    enabled: catalogBusy
                                    onClicked: skyContextController.cancelActiveDownload()
                                }

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceClearCacheButton_" + index
                                    text: "Clear cache"
                                    visible: !bundled
                                    enabled: !catalogBusy && !busy
                                    onClicked: skyContextController.clearCatalogSourceCache(instanceId)
                                }

                                PreferencesCompactActionButton {
                                    theme: catalogSection.theme
                                    objectName: "catalogSourceRemoveButton_" + index
                                    text: "Remove"
                                    visible: !bundled
                                    enabled: !catalogBusy
                                    onClicked: skyContextController.removeCatalogSource(instanceId)
                                }
                            }
                        }
                    }
                }

                PreferencesGroupTitle {
                    theme: catalogSection.theme
                    Layout.topMargin: 8
                    text: "Add Catalog Source"
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    PreferencesComboBox {
                        theme: catalogSection.theme
                        id: sourcePresetCombo
                        objectName: "catalogSourcePresetCombo"
                        Layout.fillWidth: true
                        model: skyContextController.catalogSourcePresetModel
                        textRole: "title"
                    }

                    PreferencesTextField {
                        theme: catalogSection.theme
                        id: sourceUrlInput
                        objectName: "catalogSourceUrlInput"
                        Layout.fillWidth: true
                        visible: catalogSection.customPresetSelected
                        placeholderText: "https://example.com/catalog.csv"
                        Component.onCompleted: cursorPosition = 0
                    }
                }

                PreferencesActionButton {
                    theme: catalogSection.theme
                    id: addSourceButton
                    objectName: "catalogAddSourceButton"
                    Layout.preferredWidth: 116
                    text: "Add"
                    enabled: !catalogBusy && catalogSection.canAddSelectedSource
                    onClicked: catalogSection.addSelectedSource()
                }
            }
        }
    }

    readonly property bool customPresetSelected: sourcePresetCombo.currentIndex >= 0
        && skyContextController.catalogSourcePresetIsCustom(sourcePresetCombo.currentIndex)

    readonly property bool canAddSelectedSource: sourcePresetCombo.currentIndex >= 0
        && (customPresetSelected ? sourceUrlInput.text.trim().length > 0 : true)

    function addSelectedSource() {
        if (sourcePresetCombo.currentIndex < 0) {
            return
        }

        if (customPresetSelected) {
            skyContextController.addCatalogSourceUrl(
                sourceUrlInput.text,
                skyContextController.catalogSourcePresetCategory(sourcePresetCombo.currentIndex)
            )
        } else {
            skyContextController.addCatalogSourcePreset(
                skyContextController.catalogSourcePresetId(sourcePresetCombo.currentIndex)
            )
        }
    }
}
