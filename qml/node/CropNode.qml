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
                        text: "" + root.x
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.x
                            root.x = v
                            text = "" + root.x
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!xIn.activeFocus) xIn.text = "" + root.x }
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
                        text: "" + root.y
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.y
                            root.y = v
                            text = "" + root.y
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!yIn.activeFocus) yIn.text = "" + root.y }
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
                        text: "" + root.w
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.w
                            root.w = v
                            text = "" + root.w
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!wIn.activeFocus) wIn.text = "" + root.w }
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
                        text: "" + root.h
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.h
                            root.h = v
                            text = "" + root.h
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!hIn.activeFocus) hIn.text = "" + root.h }
                        }
                    }
                }
            }

            Text {
                text: "宽/高为 0 时取到边界"
                color: Theme.fgDim
                font.pixelSize: 9
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }
    }
}
