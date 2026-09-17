import QtQuick
import Theme

Row {
    id: swatches

    property var choices: []
    property string current: ""
    signal picked(string c)

    spacing: 7

    Repeater {
        model: swatches.choices
        delegate: Rectangle {
            required property string modelData

            width: 22
            height: 22
            radius: 6
            color: modelData
            border.width: 2
            border.color: swatches.current === ("" + modelData).toLowerCase() ? Theme.fgBright : "transparent"

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: swatches.picked(modelData)
            }
        }
    }
}
