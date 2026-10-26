import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import com.skygate.app 1.0

StatusPopupScaffold {
    id: degradationPopup
    objectName: "degradationPopup"
    cardWidth: 360
    cardMaxHeight: 280
    cardTitle: "Ephemeris Degraded"

    required property var sceneModel
    readonly property int reasonCount: sceneModel.ephemerisDegradationReasons
        ? sceneModel.ephemerisDegradationReasons.length
        : 0

    onReasonCountChanged: {
        if (opened && reasonCount === 0) {
            close()
        }
    }

    Flickable {
        objectName: "degradationPopupReasonFlickable"
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(220, reasonsColumn.implicitHeight)
        contentWidth: width
        contentHeight: reasonsColumn.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: reasonsColumn
            objectName: "degradationPopupReasonColumn"
            width: parent.width
            spacing: 6

            Repeater {
                model: degradationPopup.sceneModel.ephemerisDegradationReasons

                RowLayout {
                    objectName: "degradationPopupReasonRow"
                    width: reasonsColumn.width
                    spacing: 7

                    Label {
                        objectName: "degradationPopupReasonBullet"
                        text: "!"
                        color: theme.errorText
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 1
                    }

                    Label {
                        objectName: "degradationPopupReasonText"
                        Layout.fillWidth: true
                        text: modelData
                        color: theme.textSecondary
                        font.family: "Avenir Next"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
