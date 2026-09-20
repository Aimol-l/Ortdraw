import Settings
import QtQuick
import Theme

Rectangle {
    id: badge

    property string text

    implicitWidth: badgeText.implicitWidth + 14
    implicitHeight: 20
    radius: 5
    color: Theme.bgHover

    Text {
        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
        id: badgeText
        anchors.centerIn: parent
        text: badge.text
        color: Theme.fgDim
        font.pixelSize: 10
        font.family: "monospace"
    }
}
