import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: engineSection
    required property var skyContextController
    required property var preferencesDraft
    readonly property var theme: skyContextController.theme

    function controllerText(propertyName, fallback) {
        const value = skyContextController[propertyName];
        return value === undefined || value === null ? fallback : value;
    }

    function controllerBool(propertyName, fallback) {
        const value = skyContextController[propertyName];
        return value === undefined || value === null ? fallback : value;
    }

    function supportsCorrections() {
        return skyContextController.ephemerisEngineSupportsCorrections(preferencesDraft.ephemerisEngineKindIndex);
    }

    function supportsAtmosphereSettings() {
        return skyContextController.ephemerisEngineSupportsAtmosphereSettings(preferencesDraft.ephemerisEngineKindIndex);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

            GridLayout {
                width: engineSection.width
                columns: 2
                rowSpacing: 6
                columnSpacing: 8

                PreferencesGroupTitle {
                    theme: engineSection.theme
                    columnSpan: 2
                    text: "Ephemeris Engine"
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Engine"
                    textPixelSize: 10
                }

                PreferencesComboBox {
                    theme: engineSection.theme
                    id: ephemerisEngineCombo
                    objectName: "ephemerisEngineSelectorCombo"
                    Layout.fillWidth: true
                    model: skyContextController.ephemerisEngineLabels

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(ephemerisEngineCombo.count - 1, preferencesDraft.ephemerisEngineKindIndex))
                    }

                    onActivated: {
                        preferencesDraft.ephemerisEngineKindIndex = currentIndex;
                        preferencesDraft.ephemerisCorrectionPresetIndex =
                            skyContextController.ephemerisEngineDefaultCorrectionPresetIndex(currentIndex);
                        preferencesDraft.ephemerisRefractionEnabled =
                            skyContextController.ephemerisEngineDefaultRefractionEnabled(currentIndex);
                    }
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.controllerText("ephemerisEngineStatusText", "") !== ""
                    text: "Status"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesValueLabel {
                    theme: engineSection.theme
                    objectName: "ephemerisEngineStatusLabel"
                    visible: engineSection.controllerText("ephemerisEngineStatusText", "") !== ""
                    text: engineSection.controllerText("ephemerisEngineStatusText", "")
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsCorrections()
                    text: "Corrections"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesComboBox {
                    theme: engineSection.theme
                    id: ephemerisCorrectionCombo
                    objectName: "ephemerisCorrectionPresetCombo"
                    visible: engineSection.supportsCorrections()
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    model: ["Geometric", "Astrometric", "Apparent", "Topocentric apparent"]

                    Binding on currentIndex {
                        value: Math.max(0, Math.min(ephemerisCorrectionCombo.count - 1, preferencesDraft.ephemerisCorrectionPresetIndex))
                    }

                    onActivated: preferencesDraft.ephemerisCorrectionPresetIndex = currentIndex
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsAtmosphereSettings()
                    text: "Refraction"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                RowLayout {
                    id: ephemerisRefractionRow
                    visible: engineSection.supportsAtmosphereSettings()
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    spacing: 7

                    PreferencesCheckBox {
                        theme: engineSection.theme
                        objectName: "ephemerisRefractionCheckBox"
                        visible: ephemerisRefractionRow.visible
                        checked: preferencesDraft.ephemerisRefractionEnabled
                        onToggled: preferencesDraft.ephemerisRefractionEnabled = checked
                    }

                    PreferencesFieldLabel {
                        theme: engineSection.theme
                        text: "Apply atmospheric refraction"
                    }
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    text: "Pressure"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesTextField {
                    theme: engineSection.theme
                    objectName: "ephemerisPressureInput"
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisAtmosphericPressureText
                    suffix: "hPa"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisAtmosphericPressureText = text
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    text: "Temperature"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesTextField {
                    theme: engineSection.theme
                    objectName: "ephemerisTemperatureInput"
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisAtmosphericTemperatureText
                    suffix: "C"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisAtmosphericTemperatureText = text
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    text: "Humidity"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesTextField {
                    theme: engineSection.theme
                    objectName: "ephemerisHumidityInput"
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisRelativeHumidityText
                    suffix: "%"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisRelativeHumidityText = text
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    text: "Wavelength"
                    textPixelSize: 10
                    Layout.preferredHeight: visible ? implicitHeight : 0
                }

                PreferencesTextField {
                    theme: engineSection.theme
                    objectName: "ephemerisWavelengthInput"
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: preferencesDraft.ephemerisWavelengthText
                    suffix: "um"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onEditingFinished: preferencesDraft.ephemerisWavelengthText = text
                }

                PreferencesGroupTitle {
                    theme: engineSection.theme
                    columnSpan: 2
                    Layout.topMargin: 8
                    text: "Planetary Kernels"
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Installed"
                    textPixelSize: 10
                }

                PreferencesValueLabel {
                    theme: engineSection.theme
                    objectName: "ephemerisShortRangeKernelStatusLabel"
                    text: engineSection.controllerText("ephemerisKernelDisplayText", "Bundled")
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Download"
                    textPixelSize: 10
                }

                PreferencesComboBox {
                    theme: engineSection.theme
                    id: ephemerisKernelDownloadCombo
                    objectName: "ephemerisKernelDownloadCombo"
                    Layout.fillWidth: true
                    model: ["Select kernel...", "DE440s", "DE441"]
                    enabled: engineSection.controllerBool("ephemerisDataUpdateEnabled", false)

                    Binding on currentIndex {
                        value: 0
                    }

                    onActivated: function(index) {
                        if (index === 1) {
                            skyContextController.updateEphemerisDataProfile("de440s-short-range");
                        } else if (index === 2) {
                            skyContextController.updateEphemerisDataProfile("de441-long-range");
                        }
                        ephemerisKernelDownloadCombo.currentIndex = 0;
                    }
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Cache"
                    textPixelSize: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    PreferencesValueLabel {
                        theme: engineSection.theme
                        objectName: "ephemerisPlanetaryKernelCacheSizeLabel"
                        text: engineSection.controllerText("ephemerisPlanetaryKernelCacheSizeText", "0 B")
                    }

                    PreferencesActionButton {
                        theme: engineSection.theme
                        objectName: "ephemerisPlanetaryKernelClearCacheButton"
                        Layout.preferredWidth: 86
                        text: "Clear"
                        onClicked: skyContextController.clearPlanetaryKernelCache()
                    }
                }

                PreferencesGroupTitle {
                    theme: engineSection.theme
                    columnSpan: 2
                    Layout.topMargin: 8
                    text: "Time and Earth Data"
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Earth orientation"
                    textPixelSize: 10
                }

                PreferencesValueLabel {
                    theme: engineSection.theme
                    objectName: "ephemerisEarthOrientationStatusLabel"
                    text: engineSection.controllerText("ephemerisEarthOrientationStatusText", "Not installed")
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Leap seconds"
                    textPixelSize: 10
                }

                PreferencesValueLabel {
                    theme: engineSection.theme
                    objectName: "ephemerisLeapSecondStatusLabel"
                    text: engineSection.controllerText("ephemerisLeapSecondStatusText", "Not installed")
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Delta T"
                    textPixelSize: 10
                }

                PreferencesValueLabel {
                    theme: engineSection.theme
                    objectName: "ephemerisDeltaTStatusLabel"
                    text: engineSection.controllerText("ephemerisDeltaTStatusText", "Not installed")
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Download"
                    textPixelSize: 10
                }

                PreferencesComboBox {
                    theme: engineSection.theme
                    id: ephemerisSupportDataDownloadCombo
                    objectName: "ephemerisSupportDataDownloadCombo"
                    Layout.fillWidth: true
                    model: ["Select data...", "Latest", "Info only"]
                    enabled: engineSection.controllerBool("ephemerisDataUpdateEnabled", false)

                    Binding on currentIndex {
                        value: 0
                    }

                    onActivated: function(index) {
                        if (index === 1) {
                            skyContextController.updateEphemerisDataProfile("support-data");
                        } else if (index === 2) {
                            skyContextController.checkEphemerisSupportDataUpdates();
                        }
                        ephemerisSupportDataDownloadCombo.currentIndex = 0;
                    }
                }

                PreferencesFieldLabel {
                    theme: engineSection.theme
                    text: "Cache"
                    textPixelSize: 10
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    PreferencesValueLabel {
                        theme: engineSection.theme
                        objectName: "ephemerisSupportDataCacheSizeLabel"
                        text: engineSection.controllerText("ephemerisSupportDataCacheSizeText", "0 B")
                    }

                    PreferencesActionButton {
                        theme: engineSection.theme
                        objectName: "ephemerisSupportDataClearCacheButton"
                        Layout.preferredWidth: 86
                        text: "Clear"
                        onClicked: skyContextController.clearSupportDataCache()
                    }
                }
            }
        }
    }
}
