import QtQuick
import TensorNode
import Theme
import Settings

TensorNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    function fmt(v) {
        return "" + (Math.round(v * 1000) / 1000)
    }

    readonly property real cellWidth: Math.max(28, Math.floor((card.width - 40) / root.cols))

    readonly property var presets: [
        { name: "单位",     m: [0,0,0, 0,1,0, 0,0,0] },
        { name: "均值",     m: [1,1,1, 1,1,1, 1,1,1], scale: 1/9 },
        { name: "高斯",     m: [1,2,1, 2,4,2, 1,2,1], scale: 1/16 },
        { name: "锐化",     m: [0,-1,0, -1,5,-1, 0,-1,0] },
        { name: "拉普拉斯", m: [0,1,0, 1,-4,1, 0,1,0] },
        { name: "边缘",     m: [-1,-1,-1, -1,8,-1, -1,-1,-1] },
        { name: "Sobel X",  m: [-1,0,1, -2,0,2, -1,0,1] },
        { name: "Sobel Y",  m: [-1,-2,-1, 0,0,0, 1,2,1] },
        { name: "浮雕",     m: [-2,-1,0, -1,1,1, 0,1,2] }
    ]

    function applyPreset(i) {
        var p = root.presets[i]
        root.setShape(3, 3)
        var s = p.scale !== undefined ? p.scale : 1
        for (var r = 0; r < 3; ++r)
            for (var c = 0; c < 3; ++c)
                root.setValue(r, c, p.m[r * 3 + c] * s)
    }

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: ""

        Column {
            width: parent.width
            spacing: 6

            // ---- 预设 ----
            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: root.presets
                    delegate: Rectangle {
                        required property var modelData
                        required property int index
                        width: 52
                        height: 20
                        radius: 5
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: modelData.name
                            color: Theme.fg
                            font.pixelSize: 10
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.applyPreset(index)
                        }
                    }
                }
            }

            // ---- 形状 ----
            Row {
                spacing: 10

                Row {
                    spacing: 3
                    Text {
                        height: 18
                        verticalAlignment: Text.AlignVCenter
                        text: "行"
                        color: Theme.fgDim
                        font.pixelSize: 10
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }
                    Rectangle {
                        width: 18; height: 18; radius: 4
                        color: "transparent"; border.width: 1; border.color: Theme.border
                        Text { anchors.centerIn: parent; text: "−"; color: Theme.fg; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.setShape(root.rows - 1, root.cols)
                        }
                    }
                    Text {
                        width: 14; height: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: "" + root.rows
                        color: Theme.fg
                        font.pixelSize: 11
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }
                    Rectangle {
                        width: 18; height: 18; radius: 4
                        color: "transparent"; border.width: 1; border.color: Theme.border
                        Text { anchors.centerIn: parent; text: "+"; color: Theme.fg; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.setShape(root.rows + 1, root.cols)
                        }
                    }
                }

                Row {
                    spacing: 3
                    Text {
                        height: 18
                        verticalAlignment: Text.AlignVCenter
                        text: "列"
                        color: Theme.fgDim
                        font.pixelSize: 10
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }
                    Rectangle {
                        width: 18; height: 18; radius: 4
                        color: "transparent"; border.width: 1; border.color: Theme.border
                        Text { anchors.centerIn: parent; text: "−"; color: Theme.fg; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.setShape(root.rows, root.cols - 1)
                        }
                    }
                    Text {
                        width: 14; height: 18
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: "" + root.cols
                        color: Theme.fg
                        font.pixelSize: 11
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }
                    Rectangle {
                        width: 18; height: 18; radius: 4
                        color: "transparent"; border.width: 1; border.color: Theme.border
                        Text { anchors.centerIn: parent; text: "+"; color: Theme.fg; font.pixelSize: 12 }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.setShape(root.rows, root.cols + 1)
                        }
                    }
                }
            }

            // ---- 数值网格 ----
            Grid {
                columns: root.cols
                spacing: 2

                Repeater {
                    model: root.rows * root.cols
                    delegate: Rectangle {
                        required property int index
                        readonly property int r: Math.floor(index / root.cols)
                        readonly property int c: index % root.cols

                        width: root.cellWidth
                        height: 22
                        radius: 4
                        color: Theme.bg
                        border.width: 1
                        border.color: cell.activeFocus ? Theme.blue : Theme.border

                        TextInput {
                            id: cell
                            anchors.fill: parent
                            horizontalAlignment: TextInput.AlignHCenter
                            verticalAlignment: TextInput.AlignVCenter
                            color: Theme.fg
                            font.pixelSize: 10
                            selectByMouse: true
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            text: root.fmt(root.value(r, c))
                            onEditingFinished: {
                                var v = parseFloat(text)
                                if (isNaN(v)) v = root.value(r, c)
                                root.setValue(r, c, v)
                                text = root.fmt(root.value(r, c))
                            }
                            Connections {
                                target: root
                                function onParamsChanged() {
                                    if (!cell.activeFocus)
                                        cell.text = root.fmt(root.value(r, c))
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
