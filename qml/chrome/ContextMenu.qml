import QtQuick
import QtQuick.Controls
import Theme
import NodeManager
import UiBus
import NodeCatalog

Menu {
    id: root

    property string kind: "canvas"
    property var data: ({})
    property var nodeLayer

    signal fitRequested()
    signal addNodeRequested(string type)
    signal cloneRequested(var uid)

    width: 200
    padding: 5
    margins: 0

    // The only item generated from the default delegate is the "添加节点"
    // sub-menu trigger, so style it here and let the sub-menu decide when the
    // trigger is allowed (i.e. only for the canvas context).
    delegate: CtxItem {
        id: delegateItem
        glyph: "＋"
        visible: delegateItem.subMenu ? delegateItem.subMenu.allowed : true
    }

    background: Rectangle {
        implicitWidth: 200
        color: Theme.bgElev
        border.width: 1
        border.color: Theme.border
        radius: 9
    }

    component CtxItem: MenuItem {
        id: ci

        property string glyph: ""
        property bool danger: false

        implicitWidth: 190
        implicitHeight: 30
        padding: 0

        contentItem: Item {
            implicitWidth: 174
            implicitHeight: 30

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 9

                Text {
                    width: 14
                    anchors.verticalCenter: parent.verticalCenter
                    text: ci.glyph
                    color: ci.danger && ci.hovered ? Theme.red : Theme.fgDim
                    font.pixelSize: 13
                    horizontalAlignment: Text.AlignHCenter
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: ci.text
                    color: ci.hovered ? (ci.danger ? Theme.red : Theme.fgBright) : Theme.fg
                    font.pixelSize: 12
                }
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                visible: ci.subMenu !== null && ci.subMenu !== undefined
                text: "▸"
                color: Theme.fgDim
                font.pixelSize: 11
            }
        }

        background: Rectangle {
            radius: 6
            color: ci.hovered
                   ? (ci.danger ? Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.12)
                                : Theme.bgHover)
                   : "transparent"
        }
    }

    component CtxSeparator: MenuSeparator {
        implicitWidth: 190
        implicitHeight: 9
        padding: 0
        topPadding: 4
        bottomPadding: 4

        contentItem: Rectangle {
            implicitWidth: 174
            implicitHeight: 1
            color: Theme.borderSoft
        }
    }

    // ---- node context ----
    CtxItem {
        text: "复制节点"
        glyph: "⧉"
        visible: root.kind === "node"
        onTriggered: root.cloneNode(root.data.uid)
    }
    CtxItem {
        text: "断开全部连接"
        glyph: "⎇"
        visible: root.kind === "node"
        onTriggered: NodeManager.disconnectNode(root.data.uid)
    }
    CtxItem {
        text: "置顶"
        glyph: "⤒"
        visible: root.kind === "node"
        onTriggered: NodeManager.bringToFront(root.data.uid)
    }
    CtxSeparator { visible: root.kind === "node" }
    CtxItem {
        text: "删除节点"
        glyph: "⌫"
        danger: true
        visible: root.kind === "node"
        onTriggered: NodeManager.removeNode()
    }

    // ---- edge context ----
    CtxItem {
        text: "删除连线"
        glyph: "⌫"
        danger: true
        visible: root.kind === "edge"
        onTriggered: NodeManager.removeEdge()
    }

    // ---- canvas context ----
    Menu {
        id: addMenu
        title: "添加节点"
        property bool allowed: root.kind === "canvas"

        Instantiator {
            model: NodeCatalog.items
            delegate: CtxItem {
                text: modelData.title
                glyph: "◇"
                onTriggered: root.addNode(modelData.type)
            }
        }
    }
    CtxItem {
        text: "适应视图"
        glyph: "⤢"
        visible: root.kind === "canvas"
        onTriggered: root.fitRequested()
    }
    CtxSeparator { visible: root.kind === "canvas" }
    CtxItem {
        text: "清空画布"
        glyph: "⌦"
        danger: true
        visible: root.kind === "canvas"
        onTriggered: NodeManager.clearGraph()
    }

    function openAt(x, y, kind, data) {
        root.kind = kind
        root.data = data || ({})
        root.popup(x, y)
    }

    function addNode(type) {
        var url = NodeCatalog.componentUrl(type)
        if (!url) {
            console.warn("节点类型尚未实现，无法添加:", type)
            return null
        }
        if (!nodeLayer) {
            console.warn("ContextMenu.nodeLayer 未注入，无法添加节点")
            return null
        }
        var comp = Qt.createComponent(url)
        if (comp.status !== Component.Ready) {
            console.warn("节点组件加载失败:", url, comp.errorString())
            return null
        }
        var obj = comp.createObject(nodeLayer, { x: 160, y: 140 })
        if (!obj) {
            console.warn("节点对象创建失败:", type)
            return null
        }
        if (!NodeManager.createNode(obj)) {
            console.warn("NodeManager.createNode 失败:", type)
            obj.destroy()
            return null
        }
        root.addNodeRequested(type)
        return obj
    }

    function cloneNode(uid) {
        var src = NodeManager.selectedNode
        if (!src)
            return null
        var url = NodeCatalog.componentUrl(src.typeName)
        if (!url) {
            console.warn("该节点类型暂不支持复制:", src.typeName)
            return null
        }
        if (!nodeLayer) {
            console.warn("ContextMenu.nodeLayer 未注入，无法复制节点")
            return null
        }
        var comp = Qt.createComponent(url)
        if (comp.status !== Component.Ready) {
            console.warn("节点组件加载失败:", url, comp.errorString())
            return null
        }
        var obj = comp.createObject(nodeLayer, { x: src.x + 28, y: src.y + 28 })
        if (!obj) {
            console.warn("节点对象创建失败:", src.typeName)
            return null
        }
        if (!NodeManager.createNode(obj)) {
            console.warn("NodeManager.createNode 失败:", src.typeName)
            obj.destroy()
            return null
        }
        root.cloneRequested(uid)
        return obj
    }

    Connections {
        target: UiBus
        function onContextMenuRequested(x, y, kind, data) {
            root.openAt(x, y, kind, data)
        }
    }
}
