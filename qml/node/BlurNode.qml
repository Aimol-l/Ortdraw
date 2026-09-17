import QtQuick
import BlurNode
import Theme
import Settings

BlurNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Row {
            width: parent.width
            spacing: 6

            Text {
                height: 22
                verticalAlignment: Text.AlignVCenter
                text: "核大小"
                color: Theme.fg
                font.pixelSize: 11
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
            Rectangle {
                width: 56
                height: 22
                radius: 5
                color: Theme.bg
                border.width: 1
                border.color: kIn.activeFocus ? Theme.blue : Theme.border
                TextInput {
                    id: kIn
                    anchors.fill: parent
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.fg
                    font.pixelSize: 11
                    selectByMouse: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    text: "" + root.kernel
                    onEditingFinished: {
                        var v = parseInt(text, 10)
                        if (isNaN(v)) v = root.kernel
                        root.kernel = v
                        text = "" + root.kernel
                    }
                    Connections {
                        target: root
                        function onParamsChanged() { if (!kIn.activeFocus) kIn.text = "" + root.kernel }
                    }
                }
            }
            Text {
                height: 22
                verticalAlignment: Text.AlignVCenter
                text: "（奇数）"
                color: Theme.fgDim
                font.pixelSize: 10
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }
    }
}
