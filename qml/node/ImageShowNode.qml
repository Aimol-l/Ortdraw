import QtQuick
import ImageShowNode
import Settings

ImageShowNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()
    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"
        // 图片显示要看到完整画面：按比例缩放，不裁切
        previewAspectFit: true
    }
}
