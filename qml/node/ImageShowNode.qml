import QtQuick
import ImageShowNode

ImageShowNode {
    id: root
    width: 220
    height: 300
    NodeCard { anchors.fill: parent; node: root; coordItem: root.parent }
}
