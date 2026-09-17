import QtQuick
import Theme

Item {
    id: gt

    property string text
    readonly property bool isGroup: true

    width: parent ? parent.width : 0
    implicitHeight: 42
    height: implicitHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderSoft
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 9
        text: gt.text
        color: Theme.fgDim
        font.pixelSize: 11
        font.bold: true
        font.letterSpacing: 1
    }
}
