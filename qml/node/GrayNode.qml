import QtQuick
import GrayNode
import Settings

GrayNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"
    }
}
