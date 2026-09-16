import QtQuick
import QtQuick.Controls
import ThresholdNode
import Theme

ThresholdNode {
    id: root
    width: 220
    height: 300

    NodeCard {
        anchors.fill: parent
        node: root
        coordItem: root.parent

        Column {
            width: parent.width
            spacing: 4

            Row {
                spacing: 6
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "阈值"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.threshold
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Text.CurveRendering
                }
            }

            Slider {
                id: slider
                width: parent.width
                from: 0
                to: 255
                stepSize: 1
                value: root.threshold
                onMoved: root.threshold = Math.round(value)
                Connections {
                    target: root
                    function onParamsChanged() {
                        if (!slider.pressed) slider.value = root.threshold
                    }
                }
            }
        }
    }
}
