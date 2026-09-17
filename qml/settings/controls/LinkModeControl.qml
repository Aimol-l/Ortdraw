import QtQuick
import Theme
import Settings

Row {
    id: linkModes

    property string value: "spline"
    signal picked(string v)

    spacing: 12

    Repeater {
        model: [
            { mode: "spline", label: "Spline" },
            { mode: "linear", label: "Linear" },
            { mode: "straight", label: "Straight" }
        ]

        delegate: Rectangle {
            required property var modelData

            width: 84
            height: 66
            radius: 8
            color: Theme.bg
            border.width: 1
            border.color: linkModes.value === modelData.mode ? Theme.blue : Theme.border

            Canvas {
                id: cvs
                anchors.top: parent.top
                anchors.topMargin: 6
                anchors.horizontalCenter: parent.horizontalCenter
                width: 72
                height: 42

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = (linkModes.value === modelData.mode) ? Theme.blue : Theme.wire
                    ctx.lineWidth = 2
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    if (modelData.mode === "spline") {
                        ctx.moveTo(6, 11)
                        ctx.bezierCurveTo(34, 11, 44, 31, 66, 31)
                    } else if (modelData.mode === "linear") {
                        ctx.moveTo(6, 11)
                        ctx.lineTo(28, 11)
                        ctx.quadraticCurveTo(34, 11, 34, 18)
                        ctx.lineTo(34, 24)
                        ctx.quadraticCurveTo(34, 31, 40, 31)
                        ctx.lineTo(66, 31)
                    } else {
                        ctx.moveTo(6, 11)
                        ctx.lineTo(66, 31)
                    }
                    ctx.stroke()
                }

                Connections {
                    target: Theme
                    function onChanged() { cvs.requestPaint() }
                }
                Connections {
                    target: Settings
                    function onRenderModeChanged() { cvs.requestPaint() }
                }
                Connections {
                    target: linkModes
                    function onValueChanged() { cvs.requestPaint() }
                }
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 4
                anchors.horizontalCenter: parent.horizontalCenter
                text: modelData.label
                color: linkModes.value === modelData.mode ? Theme.blue : Theme.fgDim
                font.pixelSize: 11
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: linkModes.picked(modelData.mode)
            }
        }
    }
}
