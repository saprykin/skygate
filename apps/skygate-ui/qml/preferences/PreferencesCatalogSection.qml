import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: catalogSection
    required property var skyContextController
    required property var preferencesDraft
    readonly property bool catalogBusy: skyContextController.downloadingCatalog || skyContextController.catalogProcessing

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            GridLayout {
                width: catalogSection.width
                columns: 2
                rowSpacing: 6
                columnSpacing: 8

                PreferencesGroupTitle {
                    columnSpan: 2
                    text: "Ephemeris Engine"
                }

                Label {
                    text: "Engine"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesComboBox {
                    id: ephemerisEngineCombo
                    objectName: "ephemerisEngineSelectorCombo"
                    Layout.fillWidth: true
                    model: ["Simple", "High precision"]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(ephemerisEngineCombo.count - 1, preferencesDraft.ephemerisEngineKindIndex))
                    }

                    onActivated: {
                        preferencesDraft.ephemerisEngineKindIndex = currentIndex;
                        if (currentIndex === 0) {
                            preferencesDraft.ephemerisCorrectionPresetIndex = 0;
                            preferencesDraft.ephemerisRefractionEnabled = false;
                        } else if (preferencesDraft.ephemerisCorrectionPresetIndex === 0) {
                            preferencesDraft.ephemerisCorrectionPresetIndex = 3;
                            preferencesDraft.ephemerisRefractionEnabled = true;
                        }
                    }
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Corrections"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesComboBox {
                    id: ephemerisCorrectionCombo
                    objectName: "ephemerisCorrectionPresetCombo"
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    model: ["Geometric", "Astrometric", "Apparent", "Topocentric apparent"]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(ephemerisCorrectionCombo.count - 1, preferencesDraft.ephemerisCorrectionPresetIndex))
                    }

                    onActivated: preferencesDraft.ephemerisCorrectionPresetIndex = currentIndex
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Refraction"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    id: ephemerisRefractionRow
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    spacing: 7

                    PreferencesCheckBox {
                        objectName: "ephemerisRefractionCheckBox"
                        visible: ephemerisRefractionRow.visible
                        checked: preferencesDraft.ephemerisRefractionEnabled
                        onToggled: preferencesDraft.ephemerisRefractionEnabled = checked
                    }

                    Label {
                        text: "Apply atmospheric refraction"
                        color: skyContext.theme.formLabelText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Pressure hPa"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesTextField {
                    objectName: "ephemerisPressureInput"
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisAtmosphericPressureText
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisAtmosphericPressureText = text
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Temperature C"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesTextField {
                    objectName: "ephemerisTemperatureInput"
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisAtmosphericTemperatureText
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisAtmosphericTemperatureText = text
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Humidity"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesTextField {
                    objectName: "ephemerisHumidityInput"
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisRelativeHumidityText
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisRelativeHumidityText = text
                }

                Label {
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Wavelength um"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                PreferencesTextField {
                    objectName: "ephemerisWavelengthInput"
                    visible: preferencesDraft.ephemerisEngineKindIndex === 1 && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisWavelengthText
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisWavelengthText = text
                }

                PreferencesGroupTitle {
                    columnSpan: 2
                    Layout.topMargin: 8
                    text: "Ephemeris Data"
                }

                Label {
                    text: "Modern kernel"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisModernKernelStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisModernKernelStatusText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "DE441 long range"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisLongRangeStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisLongRangeKernelStatusText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Earth orientation"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisEarthOrientationStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisEarthOrientationStatusText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Leap seconds"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisLeapSecondStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisLeapSecondStatusText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Delta T"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisDeltaTStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisDeltaTStatusText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Last update"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisLastUpdateStatusLabel"
                    Layout.fillWidth: true
                    text: skyContextController.ephemerisDataLastUpdateResultText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Updates"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    PreferencesCheckBox {
                        objectName: "ephemerisDataOnlineUpdatesCheckBox"
                        checked: skyContextController.ephemerisDataOnlineUpdatesEnabled
                        onToggled: skyContextController.setEphemerisDataOnlineUpdatesEnabled(checked)
                    }

                    Label {
                        text: "Online updates"
                        color: skyContext.theme.formLabelText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Label {
                    text: "Actions"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    PreferencesActionButton {
                        objectName: "ephemerisDataUpdateButton"
                        Layout.preferredWidth: 130
                        text: "Update Modern"
                        enabled: skyContextController.ephemerisDataUpdateEnabled
                        onClicked: skyContextController.updateEphemerisDataProfile("modern")
                    }

                    PreferencesActionButton {
                        objectName: "ephemerisLongRangeUpdateButton"
                        Layout.preferredWidth: 130
                        text: "Install DE441"
                        enabled: skyContextController.ephemerisDataUpdateEnabled
                        onClicked: skyContextController.updateEphemerisDataProfile("de441-long-range")
                    }

                    PreferencesActionButton {
                        objectName: "ephemerisDataClearCacheButton"
                        Layout.fillWidth: true
                        text: "Clear Cache"
                        onClicked: skyContextController.clearEphemerisDataCache()
                    }
                }

                PreferencesGroupTitle {
                    columnSpan: 2
                    Layout.topMargin: 8
                    text: "Star Catalog"
                }

                PreferencesComboBox {
                    id: catalogPresetCombo
                    objectName: "starCatalogPresetCombo"
                    Layout.fillWidth: true
                    model: ["Bundled (recommended)", "HYG v4.2 stars + Stellarium lines", "Custom URL"]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(catalogPresetCombo.count - 1, preferencesDraft.catalogPresetIndex))
                    }

                    onActivated: preferencesDraft.catalogPresetIndex = currentIndex
                }

                PreferencesActionButton {
                    objectName: "starCatalogUseButton"
                    Layout.preferredWidth: 150
                    text: "Use catalog"
                    enabled: !catalogBusy && catalogPresetCombo.currentIndex !== 2
                    onClicked: {
                        preferencesDraft.catalogPresetIndex = catalogPresetCombo.currentIndex;
                        if (catalogPresetCombo.currentIndex === 0) {
                            skyContextController.loadCatalogPreset("bundled");
                        } else if (catalogPresetCombo.currentIndex === 1) {
                            preferencesDraft.catalogUrlText = "https://www.astronexus.com/downloads/catalogs/hygdata_v42.csv.gz";
                            skyContextController.loadCatalogPreset("hyg_v42");
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: skyContextController.catalogDatasetInfoText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                PreferencesActionButton {
                    objectName: "starCatalogClearCacheButton"
                    Layout.preferredWidth: 150
                    Layout.alignment: Qt.AlignVCenter
                    text: "Clear Catalog Cache"
                    enabled: !skyContextController.downloadingCatalog && !skyContextController.catalogProcessing
                    onClicked: {
                        preferencesDraft.catalogPresetIndex = 0;
                        skyContextController.loadCatalogPreset("bundled");
                        skyContextController.clearCatalogCache();
                    }
                }

                Label {
                    visible: catalogPresetCombo.currentIndex === 2
                    Layout.columnSpan: 2
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Catalog URL"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }

                PreferencesTextField {
                    id: catalogUrlInput
                    objectName: "starCatalogUrlInput"
                    visible: catalogPresetCombo.currentIndex === 2
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    Layout.fillWidth: true
                    placeholderText: "https://example.com/skygate-catalog.txt or HYG CSV URL"
                    Component.onCompleted: cursorPosition = 0

                    Binding on text {
                        when: !catalogUrlInput.activeFocus
                        restoreMode: Binding.RestoreNone
                        value: preferencesDraft.catalogUrlText
                    }

                    onTextEdited: preferencesDraft.catalogUrlText = text
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            cursorPosition = 0;
                        }
                    }
                    onTextChanged: {
                        if (!activeFocus) {
                            cursorPosition = 0;
                        }
                    }
                }

                PreferencesActionButton {
                    objectName: "starCatalogDownloadButton"
                    visible: catalogPresetCombo.currentIndex === 2
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: skyContextController.downloadingCatalog ? "Downloading..." : "Download"
                    enabled: !catalogBusy
                    onClicked: {
                        preferencesDraft.catalogPresetIndex = 2;
                        skyContextController.downloadCatalogFromUrl(preferencesDraft.catalogUrlText);
                    }
                }

                PreferencesGroupTitle {
                    columnSpan: 2
                    Layout.topMargin: 8
                    text: "Deep-Sky Catalog"
                }

                PreferencesComboBox {
                    id: deepSkyCatalogPresetCombo
                    objectName: "deepSkyCatalogPresetCombo"
                    Layout.fillWidth: true
                    model: ["Bundled Messier", "OpenNGC", "Custom URL"]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(deepSkyCatalogPresetCombo.count - 1, preferencesDraft.deepSkyCatalogPresetIndex))
                    }

                    onActivated: preferencesDraft.deepSkyCatalogPresetIndex = currentIndex
                }

                PreferencesActionButton {
                    objectName: "deepSkyCatalogUseButton"
                    Layout.preferredWidth: 150
                    text: "Use catalog"
                    enabled: !catalogBusy && deepSkyCatalogPresetCombo.currentIndex !== 2
                    onClicked: {
                        preferencesDraft.deepSkyCatalogPresetIndex = deepSkyCatalogPresetCombo.currentIndex;
                        if (deepSkyCatalogPresetCombo.currentIndex === 0) {
                            skyContextController.loadDeepSkyCatalogPreset("bundled_messier");
                        } else {
                            skyContextController.loadDeepSkyCatalogPreset("open_ngc");
                            preferencesDraft.deepSkyCatalogUrlText = skyContextController.deepSkyCatalogUrlText();
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    text: skyContextController.deepSkyCatalogInfoText
                    color: skyContext.theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                PreferencesActionButton {
                    objectName: "deepSkyCatalogClearCacheButton"
                    Layout.preferredWidth: 150
                    Layout.alignment: Qt.AlignVCenter
                    text: "Clear Catalog Cache"
                    enabled: !catalogBusy
                    onClicked: {
                        preferencesDraft.deepSkyCatalogPresetIndex = 0;
                        skyContextController.loadDeepSkyCatalogPreset("bundled_messier");
                        skyContextController.clearDeepSkyCatalogCache();
                    }
                }

                Label {
                    visible: deepSkyCatalogPresetCombo.currentIndex === 2
                    Layout.columnSpan: 2
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Catalog URL"
                    color: skyContext.theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 11
                }

                PreferencesTextField {
                    id: deepSkyCatalogUrlInput
                    objectName: "deepSkyCatalogUrlInput"
                    visible: deepSkyCatalogPresetCombo.currentIndex === 2
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    Layout.fillWidth: true
                    placeholderText: "https://raw.githubusercontent.com/mattiaverga/OpenNGC/.../NGC.csv"
                    Component.onCompleted: cursorPosition = 0

                    Binding on text {
                        when: !deepSkyCatalogUrlInput.activeFocus
                        restoreMode: Binding.RestoreNone
                        value: preferencesDraft.deepSkyCatalogUrlText
                    }

                    onTextEdited: preferencesDraft.deepSkyCatalogUrlText = text
                    onActiveFocusChanged: {
                        if (!activeFocus) {
                            cursorPosition = 0;
                        }
                    }
                    onTextChanged: {
                        if (!activeFocus) {
                            cursorPosition = 0;
                        }
                    }
                }

                PreferencesActionButton {
                    objectName: "deepSkyCatalogDownloadButton"
                    visible: deepSkyCatalogPresetCombo.currentIndex === 2
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: skyContextController.downloadingCatalog ? "Downloading..." : "Download"
                    enabled: !catalogBusy
                    onClicked: {
                        preferencesDraft.deepSkyCatalogPresetIndex = 2;
                        skyContextController.downloadDeepSkyCatalogFromUrl(preferencesDraft.deepSkyCatalogUrlText);
                    }
                }
            }
        }
    }
}
