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

                Label {
                    text: "Engine"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    visible: engineSection.controllerText("ephemerisEngineStatusText", "") !== ""
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Status"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisEngineStatusLabel"
                    visible: engineSection.controllerText("ephemerisEngineStatusText", "") !== ""
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: engineSection.controllerText("ephemerisEngineStatusText", "")
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    visible: engineSection.supportsCorrections()
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Corrections"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    visible: engineSection.supportsAtmosphereSettings()
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Refraction"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                    Label {
                        text: "Apply atmospheric refraction"
                        color: theme.formLabelText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        Layout.alignment: Qt.AlignVCenter
                    }
                }

                Label {
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Pressure"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Temperature"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Humidity"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    visible: engineSection.supportsAtmosphereSettings() && preferencesDraft.ephemerisRefractionEnabled
                    Layout.preferredHeight: visible ? implicitHeight : 0
                    text: "Wavelength"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    text: "Installed"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisShortRangeKernelStatusLabel"
                    Layout.fillWidth: true
                    text: engineSection.controllerText("ephemerisKernelDisplayText", "Bundled")
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Download"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    text: "Cache"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Label {
                        objectName: "ephemerisPlanetaryKernelCacheSizeLabel"
                        Layout.fillWidth: true
                        text: engineSection.controllerText("ephemerisPlanetaryKernelCacheSizeText", "0 B")
                        color: theme.listItemPrimaryText
                        font.pixelSize: 11
                        font.family: "Avenir Next"
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
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

                Label {
                    text: "Earth orientation"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisEarthOrientationStatusLabel"
                    Layout.fillWidth: true
                    text: engineSection.controllerText("ephemerisEarthOrientationStatusText", "Not installed")
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Leap seconds"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisLeapSecondStatusLabel"
                    Layout.fillWidth: true
                    text: engineSection.controllerText("ephemerisLeapSecondStatusText", "Not installed")
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Delta T"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                Label {
                    objectName: "ephemerisDeltaTStatusLabel"
                    Layout.fillWidth: true
                    text: engineSection.controllerText("ephemerisDeltaTStatusText", "Not installed")
                    color: theme.listItemPrimaryText
                    font.pixelSize: 11
                    font.family: "Avenir Next"
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                }

                Label {
                    text: "Download"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
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

                Label {
                    text: "Cache"
                    color: theme.formLabelText
                    font.family: "Avenir Next"
                    font.pixelSize: 10
                    Layout.alignment: Qt.AlignVCenter
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 7

                    Label {
                        objectName: "ephemerisSupportDataCacheSizeLabel"
                        Layout.fillWidth: true
                        text: engineSection.controllerText("ephemerisSupportDataCacheSizeText", "0 B")
                        color: theme.listItemPrimaryText
                        font.pixelSize: 11
                        font.family: "Avenir Next"
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
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
