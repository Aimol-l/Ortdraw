import QtQuick
import ImageLoadNode
import Settings
import Theme
import FileDialogs

ImageLoadNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()
    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        // 选中文件后立即预览该文件（执行后由 ImageStore 的真实结果接管）
        previewSource: root.path === "" || !root.pathValid
            ? "qrc:/preview_placeholder.png"
            : "file://" + encodeURI(root.path)

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 4

            Row {
                spacing: 6
                width: parent.width

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
                        text: "选择图片"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var p = FileDialogs.openImage(root.path)
                            if (p !== "")
                                root.path = p
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - pickBtn.width - parent.spacing)
                    text: root.path === "" ? "未选择" : root.path.split("/").pop()
                    color: Theme.fgDim
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
            }

            Text {
                visible: root.infoText !== ""
                width: parent.width
                text: root.infoText
                color: Theme.fgDim
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }
    }
}
