import Settings
import QtQuick
import Theme

Rectangle {
    id: primary

    property string label
    signal clicked()

    implicitWidth: primaryText.implicitWidth + 32
    implicitHeight: 32
    radius: 8
    color: primaryArea.containsMouse
           ? Qt.lighter(Theme.blue, 1.08)
           : Theme.blue

    Text {
        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
        id: primaryText
        anchors.centerIn: parent
        text: primary.label
        color: "#ffffff"
        font.pixelSize: 13
        font.bold: true
    }

    MouseArea {
        id: primaryArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: primary.clicked()
    }
}
