import QtQuick
import QtQuick.Controls
import ThresholdNode
import Theme
import Settings

ThresholdNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

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
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.threshold
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
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
