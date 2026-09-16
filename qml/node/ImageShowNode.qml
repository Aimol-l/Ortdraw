import QtQuick
import ImageShowNode

ImageShowNode {
    id: root
    width: 220
    height: Math.max(root.getMinHeight(), card.contentHeight)
    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"
    }
}
