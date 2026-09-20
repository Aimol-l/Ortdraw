import QtQuick
import FlipRotateNode
import Theme
import Settings
import NodeManager

FlipRotateNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property var modeNames: ["水平", "垂直", "90°", "180°", "270°"]

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Flow {
            width: parent.width
            spacing: 6

            Repeater {
                model: root.modeNames
                delegate: Rectangle {
                    required property int index
                    required property string modelData

                    width: chipLabel.implicitWidth + 16
                    height: 22
                    radius: 5
                    color: root.mode === index ? Theme.blue : Theme.bg
                    border.width: 1
                    border.color: root.mode === index ? Theme.blue : Theme.border

                    Text {
                        id: chipLabel
                        anchors.centerIn: parent
                        text: parent.modelData
                        color: root.mode === index ? "#ffffff" : Theme.fgDim
                        font.pixelSize: 10
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.mode = index
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }
        }
    }
}
