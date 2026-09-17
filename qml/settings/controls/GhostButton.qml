import QtQuick
import Theme

Rectangle {
    id: ghost

    property string label
    signal clicked()

    implicitWidth: ghostText.implicitWidth + 32
    implicitHeight: 32
    radius: 8
    color: ghostArea.containsMouse ? Theme.bgHover : Theme.bg
    border.width: 1
    border.color: ghostArea.containsMouse ? Theme.blue : Theme.border

    Text {
        id: ghostText
        anchors.centerIn: parent
        text: ghost.label
        color: Theme.fg
        font.pixelSize: 13
        font.bold: true
    }

    MouseArea {
        id: ghostArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: ghost.clicked()
    }
}
