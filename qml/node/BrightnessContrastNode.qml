import QtQuick
import QtQuick.Controls
import BrightnessContrastNode
import Theme
import Settings
import NodeManager

BrightnessContrastNode {
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
                    height: 18; verticalAlignment: Text.AlignVCenter
                    text: "亮度"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18; verticalAlignment: Text.AlignVCenter
                    text: "" + root.brightness
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
            }

            Slider {
                id: bSlider
                width: parent.width
                from: -100
                to: 100
                stepSize: 1
                value: root.brightness
                onMoved: root.brightness = Math.round(value)
                onPressedChanged: if (!pressed) NodeManager.commitNodeParams(root.uuid)
                Connections {
                    target: root
                    function onParamsChanged() { if (!bSlider.pressed) bSlider.value = root.brightness }
                }
            }

            Row {
                spacing: 6
                Text {
                    height: 18; verticalAlignment: Text.AlignVCenter
                    text: "对比度"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Text {
                    height: 18; verticalAlignment: Text.AlignVCenter
                    text: "" + root.contrast + "%"
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
            }

            Slider {
                id: cSlider
                width: parent.width
                from: 0
                to: 300
                stepSize: 1
                value: root.contrast
                onMoved: root.contrast = Math.round(value)
                onPressedChanged: if (!pressed) NodeManager.commitNodeParams(root.uuid)
                Connections {
                    target: root
                    function onParamsChanged() { if (!cSlider.pressed) cSlider.value = root.contrast }
                }
            }
        }
    }
}
