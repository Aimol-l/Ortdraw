import QtQuick
import MorphologyNode
import Theme
import Settings
import NodeManager

MorphologyNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property var opNames: ["腐蚀", "膨胀", "开", "闭", "梯度"]

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Column {
            width: parent.width
            spacing: 6

            Flow {
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.opNames
                    delegate: Rectangle {
                        required property int index
                        required property string modelData

                        width: chipLabel.implicitWidth + 14
                        height: 22
                        radius: 5
                        color: root.op === index ? Theme.blue : Theme.bg
                        border.width: 1
                        border.color: root.op === index ? Theme.blue : Theme.border

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: root.op === index ? "#ffffff" : Theme.fgDim
                            font.pixelSize: 10
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.op = index
                                NodeManager.commitNodeParams(root.uuid)
                            }
                        }
                    }
                }
            }

            // ---- 核大小（3..15 奇数）----
            Row {
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
                    width: 22; height: 22; radius: 5
                    color: kMinus.pressed ? Theme.bgHover : Theme.bg
                    border.width: 1; border.color: Theme.border
                    Text { anchors.centerIn: parent; text: "−"; color: Theme.fgDim; font.pixelSize: 12 }
                    MouseArea {
                        id: kMinus
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.kernel = root.kernel - 2
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }

                Text {
                    width: 26
                    height: 22
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.kernel
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Rectangle {
                    width: 22; height: 22; radius: 5
                    color: kPlus.pressed ? Theme.bgHover : Theme.bg
                    border.width: 1; border.color: Theme.border
                    Text { anchors.centerIn: parent; text: "+"; color: Theme.fgDim; font.pixelSize: 12 }
                    MouseArea {
                        id: kPlus
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.kernel = root.kernel + 2
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }

            // ---- 迭代次数（1..5）----
            Row {
                spacing: 6
                Text {
                    height: 22
                    verticalAlignment: Text.AlignVCenter
                    text: "迭代"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Rectangle {
                    width: 22; height: 22; radius: 5
                    color: iMinus.pressed ? Theme.bgHover : Theme.bg
                    border.width: 1; border.color: Theme.border
                    Text { anchors.centerIn: parent; text: "−"; color: Theme.fgDim; font.pixelSize: 12 }
                    MouseArea {
                        id: iMinus
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.iterations = root.iterations - 1
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }

                Text {
                    width: 26
                    height: 22
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    text: "" + root.iterations
                    color: Theme.blue
                    font.pixelSize: 11
                    font.bold: true
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Rectangle {
                    width: 22; height: 22; radius: 5
                    color: iPlus.pressed ? Theme.bgHover : Theme.bg
                    border.width: 1; border.color: Theme.border
                    Text { anchors.centerIn: parent; text: "+"; color: Theme.fgDim; font.pixelSize: 12 }
                    MouseArea {
                        id: iPlus
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.iterations = root.iterations + 1
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }
        }
    }
}
