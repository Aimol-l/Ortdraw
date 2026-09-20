import QtQuick
import QtQuick.Controls
import MedianNode
import Theme
import Settings
import NodeManager

MedianNode {
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
                    text: "核大小"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.kernel
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "（奇数）"
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
            }

            Slider {
                id: slider
                width: parent.width
                from: 3
                to: 15
                stepSize: 2
                value: root.kernel
                onMoved: root.kernel = Math.round(value)
                // AbstractSlider 无 released 信号，用 pressed 变 false 表示松开
                onPressedChanged: if (!pressed) NodeManager.commitNodeParams(root.uuid)
                Connections {
                    target: root
                    function onParamsChanged() { if (!slider.pressed) slider.value = root.kernel }
                }
            }
        }
    }
}
