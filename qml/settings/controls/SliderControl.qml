import Settings
import QtQuick
import Theme

Item {
    id: slider

    property int from: 0
    property int to: 100
    property int value: 0
    property string suffix: ""
    signal moved(int v)

    implicitWidth: 152
    implicitHeight: 22

    function ratio() {
        return Math.max(0, Math.min(1, (slider.value - slider.from) / (slider.to - slider.from)))
    }

    Rectangle {
        id: track
        width: 110
        height: 4
        radius: 2
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.border

        Rectangle {
            width: slider.ratio() * track.width
            height: parent.height
            radius: 2
            color: Theme.blue
        }
        Rectangle {
            width: 12
            height: 12
            radius: 6
            color: Theme.blue
            x: slider.ratio() * track.width - 6
            anchors.verticalCenter: parent.verticalCenter
        }

        MouseArea {
            anchors.fill: parent
            anchors.margins: -8
            cursorShape: Qt.PointingHandCursor

            function setFromX(mx) {
                var r = Math.max(0, Math.min(1, (mx - 8) / track.width))
                slider.moved(Math.round(slider.from + r * (slider.to - slider.from)))
            }
            onPressed: (m) => setFromX(m.x)
            onPositionChanged: (m) => { if (pressed) setFromX(m.x) }
        }
    }

    Text {
        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 34
        horizontalAlignment: Text.AlignRight
        text: slider.value + slider.suffix
        color: Theme.fg
        font.pixelSize: 12
    }
}
