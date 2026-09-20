import QtQuick
import CropNode
import Theme
import Settings
import NodeManager

CropNode {
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
            spacing: 6

            Row {
                spacing: 6

                Text {
                    width: 16; height: 22; verticalAlignment: Text.AlignVCenter
                    text: "X"; color: Theme.fg; font.pixelSize: 10
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Rectangle {
                    width: 52; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: xIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: xIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 10
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.cropX
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.cropX
                            root.cropX = v
                            text = "" + root.cropX
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!xIn.activeFocus) xIn.text = "" + root.cropX }
                        }
                    }
                }

                Text {
                    width: 16; height: 22; verticalAlignment: Text.AlignVCenter
                    text: "Y"; color: Theme.fg; font.pixelSize: 10
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Rectangle {
                    width: 52; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: yIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: yIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 10
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.cropY
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.cropY
                            root.cropY = v
                            text = "" + root.cropY
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!yIn.activeFocus) yIn.text = "" + root.cropY }
                        }
                    }
                }
            }

            Row {
                spacing: 6

                Text {
                    width: 16; height: 22; verticalAlignment: Text.AlignVCenter
                    text: "宽"; color: Theme.fg; font.pixelSize: 10
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Rectangle {
                    width: 52; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: wIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: wIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 10
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.cropW
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.cropW
                            root.cropW = v
                            text = "" + root.cropW
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!wIn.activeFocus) wIn.text = "" + root.cropW }
                        }
                    }
                }

                Text {
                    width: 16; height: 22; verticalAlignment: Text.AlignVCenter
                    text: "高"; color: Theme.fg; font.pixelSize: 10
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
                Rectangle {
                    width: 52; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: hIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: hIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 10
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.cropH
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.cropH
                            root.cropH = v
                            text = "" + root.cropH
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!hIn.activeFocus) hIn.text = "" + root.cropH }
                        }
                    }
                }
            }

            Text {
                text: "宽/高 >0；为 0 取到边界；x+w>宽 或 y+h>高 时报错"
                color: Theme.fgDim
                font.pixelSize: 9
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }
    }
}
