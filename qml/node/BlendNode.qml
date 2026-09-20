import QtQuick
import QtQuick.Controls
import BlendNode
import Theme
import Settings
import NodeManager

BlendNode {
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
                    text: "图像A"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.alpha + "%"
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
                to: 100
                stepSize: 1
                value: root.alpha
                onMoved: root.alpha = Math.round(value)
                // AbstractSlider 无 released 信号，用 pressed 变 false 表示松开
                onPressedChanged: if (!pressed) NodeManager.commitNodeParams(root.uuid)
                Connections {
                    target: root
                    function onParamsChanged() { if (!slider.pressed) slider.value = root.alpha }
                }
            }
        }
    }
}
