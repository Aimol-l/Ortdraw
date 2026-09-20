import QtQuick
import ImageSaveNode
import Settings
import Theme
import FileDialogs
import NodeManager

ImageSaveNode {
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
            spacing: 6
            width: parent ? parent.width : implicitWidth

            Rectangle {
                id: pickBtn
                anchors.verticalCenter: parent.verticalCenter
                width: pickLabel.implicitWidth + 20
                height: 24
                radius: 6
                color: pickHover.hovered ? Theme.bgHover : Theme.bgElev
                border.width: 1
                border.color: pickHover.hovered ? Theme.blue : Theme.border
                Behavior on color { ColorAnimation { duration: 120 } }

                HoverHandler { id: pickHover }

                Text {
                    id: pickLabel
                    anchors.centerIn: parent
                    text: "选择保存位置"
                    color: Theme.fg
                    font.pixelSize: 12
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var p = FileDialogs.saveImage(root.path)
                        if (p !== "") {
                            root.path = p
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, parent.width - pickBtn.width - parent.spacing)
                text: root.path === "" ? "未设置(自动保存到图片目录)" : root.path.split("/").pop()
                color: Theme.fgDim
                font.pixelSize: 11
                elide: Text.ElideMiddle
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }
    }
}
