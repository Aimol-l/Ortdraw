import QtQuick
import PreProcessNode
import Theme
import Settings

PreProcessNode {
    id: root
    width: 288
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: ""

        TaskNodeControls {
            width: parent ? parent.width : implicitWidth
            node: root
        }
    }
}
