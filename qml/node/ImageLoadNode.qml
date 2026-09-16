import QtQuick
import ImageLoadNode

ImageLoadNode {
    id: root
    width: 220
    height: 300
    NodeCard { anchors.fill: parent; node: root; coordItem: root.parent }
}
