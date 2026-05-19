import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

Item {
    id: engineSection
    required property var skyContextController
    required property var preferencesDraft

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
                    text: "Kernel"
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
            }
        }
    }
}
