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
    readonly property int topkRowHeight: 12

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
            Item {
                visible: root.hasTopk
                width: parent.width
                height: root.hasTopk ? (root.topkRowHeight + root.topkList.length * root.topkRowHeight) : 0

                Item {
                    width: parent.width
                    height: root.topkRowHeight

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "类别"
                        color: Theme.fgDim
                        font.pixelSize: 9
                        renderType: root.textRenderType
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "置信度"
                        color: Theme.fgDim
                        font.pixelSize: 9
                        renderType: root.textRenderType
                    }
                }

                Column {
                    anchors.top: parent.top
                    anchors.topMargin: root.topkRowHeight
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: root.topkList
                        delegate: Row {
                            id: barRow
                            required property var modelData
                            width: parent.width
                            height: root.topkRowHeight
                            spacing: 6

                            Text {
                                height: barRow.height
                                width: 64
                                verticalAlignment: Text.AlignVCenter
                                // 有类别文件时显示名称，否则回退 "类 <id>"
                                text: barRow.modelData.name !== undefined
                                    ? ("" + barRow.modelData.name) : "类 " + barRow.modelData.id
                                color: Theme.fg
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                renderType: root.textRenderType
                            }
                            Item {
                                width: Math.max(0, barRow.width - 64 - 34 - barRow.spacing * 2)
                                height: barRow.height

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    height: 6
                                    radius: 3
                                    color: Theme.bg
                                    clip: true

                                    Rectangle {
                                        width: parent.width * Math.max(0, Math.min(1,
                                                   barRow.modelData.score))
                                        height: parent.height
                                        radius: 3
                                        color: Theme.blue
                                    }
                                }
                            }
                            Text {
                                height: barRow.height
                                width: 34
                                verticalAlignment: Text.AlignVCenter
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
}
