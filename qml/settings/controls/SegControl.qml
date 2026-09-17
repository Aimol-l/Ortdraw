import QtQuick
import Theme

Rectangle {
    id: seg

    property var options: []
    property string value: ""
    signal picked(string v)

    implicitWidth: segRow.width + 4
    implicitHeight: 30
    radius: 8
    color: Theme.bg
    border.width: 1
    border.color: Theme.border

    Row {
        id: segRow
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: seg.options
            delegate: Rectangle {
                required property var modelData

                width: segLabel.implicitWidth + 24
                height: 24
                radius: 6
                color: seg.value === modelData.value ? Theme.blue : "transparent"

                Text {
                    id: segLabel
                    anchors.centerIn: parent
                    text: modelData.label
                    color: seg.value === modelData.value ? "#ffffff" : Theme.fgDim
                    font.pixelSize: 12
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: seg.picked(modelData.value)
                }
            }
        }
    }
}
