import QtQuick
import Theme

Rectangle {
    id: sw

    property bool checked: false
    signal toggled(bool v)

    width: 40
    height: 22
    radius: 11
    color: sw.checked ? Theme.blue : Theme.bg
    border.width: 1
    border.color: sw.checked ? Theme.blue : Theme.border

    Behavior on color { ColorAnimation { duration: 130 } }

    Rectangle {
        width: 16
        height: 16
        radius: 8
        y: 2
        x: sw.checked ? 20 : 2
        color: sw.checked ? "#ffffff" : Theme.fgDim
        Behavior on x { NumberAnimation { duration: 130 } }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: sw.toggled(!sw.checked)
    }
}
