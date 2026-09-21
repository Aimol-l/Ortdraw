import QtQuick
import PostProcessNode
import Theme
import Settings

PostProcessNode {
    id: root
    width: 288
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    // 当前任务输出是否含图像（用于决定是否显示缩略图预览）
    readonly property bool hasImageOutput: {
        var ps = root.outputPorts
        for (var i = 0; i < ps.length; ++i)
            if (ps[i].dataTypeName === "Image") return true
        return false
    }

    // displayData 中的 Top-K 列表（分类任务）
    readonly property var topkList: root.displayData ? root.displayData.topk : null
    readonly property bool hasTopk: topkList !== undefined && topkList !== null && topkList.length > 0

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: root.hasImageOutput ? "qrc:/preview_placeholder.png" : ""

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 6

            TaskNodeControls {
                width: parent.width
                node: root
            }

            // ---- 分类 Top-K 结果面板（非端口显示数据）----
            Column {
                visible: root.hasTopk
                width: parent.width
                spacing: 3

                Text {
                    text: "Top-K" + (root.displayData && root.displayData.top1 !== undefined
                                     ? "   类别 id: " + root.displayData.top1 : "")
                    color: Theme.fgBright
                    font.pixelSize: 11
                    renderType: root.textRenderType
                }

                Repeater {
                    model: root.topkList
                    delegate: Row {
                        id: barRow
                        required property var modelData
                        width: parent.width
                        spacing: 6

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 54
                            text: "类别 " + barRow.modelData.id
                            color: Theme.fg
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            renderType: root.textRenderType
                        }

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(0, barRow.width - 54 - scoreText.width
                                            - barRow.spacing * 2)
                            height: 10
                            radius: 3
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.borderSoft
                            clip: true

                            Rectangle {
                                width: parent.width * Math.max(0, Math.min(1, barRow.modelData.score))
                                height: parent.height
                                radius: 3
                                color: Theme.blue
                            }
                        }

                        Text {
                            id: scoreText
                            anchors.verticalCenter: parent.verticalCenter
                            width: 40
                            horizontalAlignment: Text.AlignRight
                            text: "" + (Math.round(barRow.modelData.score * 1000) / 1000)
                            color: Theme.fgDim
                            font.pixelSize: 10
                            renderType: root.textRenderType
                        }
                    }
                }
            }
        }
    }
}
