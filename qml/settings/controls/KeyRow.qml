import Settings
import QtQuick
import Theme

Item {
    id: kr

    property string label
    property string keys

    width: parent ? parent.width : 0
    implicitHeight: 30
    height: implicitHeight

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Theme.borderSoft
    }

    Text {
        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
        id: keyLabel
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - keyBadge.width - 10
        text: kr.label
        color: Theme.fg
        font.pixelSize: 12
        elide: Text.ElideRight
    }

    Rectangle {
        id: keyBadge
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: keyText.implicitWidth + 12
        height: 22
        radius: 5
        color: Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            id: keyText
            anchors.centerIn: parent
            text: kr.keys
            color: Theme.fgDim
            font.pixelSize: 11
            font.family: "monospace"
        }
    }
}
