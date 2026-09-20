import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import Theme
import Settings
import NodeManager
import NodeCatalog
import FileDialogs
import "settings"
import "settings/controls"

Controls.ApplicationWindow {
    id: win

    width: 1600
    height: 900
    visible: true
    minimumWidth: 900
    minimumHeight: 600
    color: Theme.bg
    title: "Ortdraw"

    property bool leftCollapsed: false
    property bool rightCollapsed: false
    property string currentPath: ""
    property string savedSnapshot: ""
    property bool allowClose: false
    property var pendingAction: null   // 未保存提示确认后要执行的动作；null 表示关闭窗口

    property real leftPanelWidth: leftCollapsed ? 0 : 250
    property real rightPanelWidth: rightCollapsed ? 0 : 286

    Behavior on leftPanelWidth { NumberAnimation { duration: 200 } }
    Behavior on rightPanelWidth { NumberAnimation { duration: 200 } }

    // Dev aid (not a product feature): when launched with --demo, create two
    // sample nodes so the UI can be verified/screenshotted without a mouse.
    function demoPopulate() {
        palette.addNodeAt("ImageLoad", 80, 120)
        palette.addNodeAt("ImageShow", 720, 200)
        savedSnapshot = NodeManager.graphJsonString()
    }

    // 与上次保存 / 打开 / 新建时的快照比较，判断是否有未保存的修改
    function graphDirty() { return NodeManager.graphJsonString() !== savedSnapshot }
    // 画布为空时不提示保存
    function needsSave() { return NodeManager.nodeCount > 0 && graphDirty() }

    // 关闭窗口前询问是否保存未保存的更改
    onClosing: (close) => {
        if (allowClose || !needsSave()) {
            close.accepted = true
            return
        }
        close.accepted = false
        promptThen(null)   // null：确认后关闭窗口
    }

    // 有未保存修改则先询问，否则直接执行 action
    function promptThen(action) {
        if (needsSave()) { pendingAction = action; closePrompt.open() }
        else action()
    }
    // 提示确认后：执行待定动作；若无（关闭场景）则关闭窗口
    function resolvePending() {
        var a = pendingAction
        pendingAction = null
        closePrompt.close()
        if (a) a()
        else { allowClose = true; win.close() }
    }

    // ---- 图文件：新建 / 打开 / 保存 ----
    function doNew() {
        NodeManager.clearGraph()
        currentPath = ""
        savedSnapshot = NodeManager.graphJsonString()
    }
    // 新建 / 打开前若有未保存修改，先询问
    function newGraph() { promptThen(doNew) }
    function openGraph() {
        var p = FileDialogs.openGraph(currentPath)
        if (p === "") return
        promptThen(function() { doLoad(p) })
    }
    function saveGraph() { if (currentPath === "") saveGraphAs(); else doSave(currentPath) }
    function saveGraphAs() { var p = FileDialogs.saveGraph(currentPath); if (p !== "") doSave(p) }
    function doSave(path) {
        if (NodeManager.saveGraph(path)) {
            currentPath = path
            savedSnapshot = NodeManager.graphJsonString()
            return true
        }
        return false
    }
    function doLoad(path) {
        var g = NodeManager.readGraph(path)
        if (!g || !g.nodes)
            return
        NodeManager.clearGraph()
        var map = ({})
        for (var i = 0; i < g.nodes.length; ++i) {
            var d = g.nodes[i]
            var url = NodeCatalog.componentUrl(d.type)
            if (!url)
                continue
            var comp = Qt.createComponent(url)
            if (comp.status !== Component.Ready) {
                console.error(comp.errorString())
                continue
            }
            var obj = comp.createObject(canvas.nodeLayer)
            if (!obj)
                continue
            if (d.uuid) obj.setUuid(d.uuid)
            obj.x = d.x
            obj.y = d.y
            if (d.w) obj.width = d.w
            if (d.h) obj.height = d.h
            if (d.name) obj.name = d.name
            if (d.params) obj.setParams(d.params)
            NodeManager.createNode(obj)
            map[d.uuid] = obj
        }
        for (var j = 0; j < g.edges.length; ++j) {
            var e = g.edges[j]
            NodeManager.addEdgeByUuid(e.fromNode, e.fromPort, e.toNode, e.toPort)
        }
        currentPath = path
        savedSnapshot = NodeManager.graphJsonString()
    }

    Component.onCompleted: {
        leftCollapsed = width < 960
        rightCollapsed = width < 1180
        savedSnapshot = NodeManager.graphJsonString()
        if (Qt.application.arguments.indexOf("--demo") >= 0)
            Qt.callLater(demoPopulate)
    }

    onWidthChanged: {
        leftCollapsed = width < 960
        rightCollapsed = width < 1180
    }

    Item {
        id: uiRoot
        width: win.width / Settings.uiScale
        height: win.height / Settings.uiScale
        transformOrigin: Item.TopLeft
        scale: Settings.uiScale
        clip: true

    Column {
        anchors.fill: parent

        TopBar {
            id: topbar
            width: parent.width
            onUndoRequested: NodeManager.undo()
            onRedoRequested: NodeManager.redo()
            onFitRequested: canvas.fitView()
            onClearRequested: NodeManager.clearGraph()
            onNewRequested: win.newGraph()
            onOpenRequested: win.openGraph()
            onSaveRequested: win.saveGraph()
            onSaveAsRequested: win.saveGraphAs()
        }

        Item {
            id: mid
            width: parent.width
            height: uiRoot.height - topbar.height - statusbar.height
            clip: true

            Row {
                id: middleRow
                anchors.fill: parent
                spacing: 0

                NodePalette {
                    id: palette
                    width: win.leftPanelWidth
                    height: middleRow.height
                    visible: win.leftPanelWidth > 0
                    z: 1
                    nodeLayer: canvas.nodeLayer
                    canvasItem: canvas
                    onCollapseRequested: win.leftCollapsed = true
                }

                CanvasArea {
                    id: canvas
                    width: middleRow.width - win.leftPanelWidth - win.rightPanelWidth
                    height: middleRow.height
                }

                Inspector {
                    width: win.rightPanelWidth
                    height: middleRow.height
                    visible: win.rightPanelWidth > 0
                    z: 1
                    onCollapseRequested: win.rightCollapsed = true
                }
            }

            Rectangle {
                id: leftRail
                visible: win.leftCollapsed
                z: 20
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 48
                topRightRadius: 8
                bottomRightRadius: 8
                color: leftRailArea.containsMouse ? Theme.bgHover : Theme.bgPanel
                border.width: 1
                border.color: leftRailArea.containsMouse ? Theme.blue : Theme.border

                Text {
                    anchors.centerIn: parent
                    text: "›"
                    color: leftRailArea.containsMouse ? Theme.blue : Theme.fgDim
                    font.pixelSize: 14
                }

                MouseArea {
                    id: leftRailArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.leftCollapsed = false
                }
            }

            Rectangle {
                id: rightRail
                visible: win.rightCollapsed
                z: 20
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 20
                height: 48
                topLeftRadius: 8
                bottomLeftRadius: 8
                color: rightRailArea.containsMouse ? Theme.bgHover : Theme.bgPanel
                border.width: 1
                border.color: rightRailArea.containsMouse ? Theme.blue : Theme.border

                Text {
                    anchors.centerIn: parent
                    text: "‹"
                    color: rightRailArea.containsMouse ? Theme.blue : Theme.fgDim
                    font.pixelSize: 14
                }

                MouseArea {
                    id: rightRailArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: win.rightCollapsed = false
                }
            }
        }

        StatusBar {
            id: statusbar
            width: parent.width
            zoom: canvas.zoom
            nodeCount: NodeManager.nodeCount
            edgeCount: NodeManager.edgeCount
            selectedName: NodeManager.selectedNode ? NodeManager.selectedNode.name : "无"
            selectedPos: NodeManager.selectedNode
                         ? (Math.round(NodeManager.selectedNode.x) + "," + Math.round(NodeManager.selectedNode.y))
                         : "-"
            onZoomInRequested: canvas.zoomIn()
            onZoomOutRequested: canvas.zoomOut()
            onZoomSetRequested: (z) => canvas.setZoom(z)
            onFitRequested: canvas.fitView()
        }
    }
    }

    Connections {
        target: NodeManager
        function onNodeFocusRequested(wx, wy) {
            canvas.panX = canvas.width / 2 - wx * canvas.zoom
            canvas.panY = canvas.height / 2 - wy * canvas.zoom
        }
    }

    ContextMenu {
        id: ctx
        nodeLayer: canvas.nodeLayer
        onFitRequested: canvas.fitView()
    }

    ImageViewer { id: viewer }

    SettingsDialog { id: settingsDialog }

    // 关闭窗口前的未保存提示
    Controls.Popup {
        id: closePrompt
        width: 380
        height: 170
        padding: 0
        modal: true
        focus: true
        closePolicy: Controls.Popup.CloseOnEscape
        x: Math.round((win.width - width) / 2)
        y: Math.round((win.height - height) / 2)

        background: Rectangle {
            color: Theme.bgPanel
            border.width: 1
            border.color: Theme.border
            radius: 12
        }

        Item {
            anchors.fill: parent
            anchors.margins: 20

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                spacing: 10

                Text {
                    text: "未保存的更改"
                    color: Theme.fgBright
                    font.pixelSize: 15
                    font.bold: true
                }

                Text {
                    width: parent.width
                    text: "当前节点图尚未保存，是否保存？"
                    color: Theme.fg
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            Row {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                spacing: 10

                GhostButton {
                    label: "取消"
                    onClicked: { pendingAction = null; closePrompt.close() }
                }

                GhostButton {
                    label: "不保存"
                    onClicked: resolvePending()
                }

                PrimaryButton {
                    label: "保存"
                    onClicked: {
                        var p = currentPath
                        if (p === "")
                            p = FileDialogs.saveGraph(currentPath)
                        if (p === "")
                            return
                        if (doSave(p))
                            resolvePending()
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Ctrl+N"
        enabled: !settingsDialog.visible
        onActivated: win.newGraph()
    }

    Shortcut {
        sequence: "Ctrl+O"
        enabled: !settingsDialog.visible
        onActivated: win.openGraph()
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: !settingsDialog.visible
        onActivated: win.saveGraph()
    }

    Shortcut {
        sequence: "Delete"
        enabled: !settingsDialog.visible
        onActivated: {
            NodeManager.removeNode()
            NodeManager.removeEdge()
        }
    }

    Shortcut {
        sequence: "Ctrl+Z"
        enabled: !settingsDialog.visible
        onActivated: NodeManager.undo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        enabled: !settingsDialog.visible
        onActivated: NodeManager.redo()
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: NodeManager.selectedNode !== null && !settingsDialog.visible
        onActivated: ctx.cloneNode(NodeManager.selectedNode.uuid)
    }

    Shortcut {
        sequence: "Ctrl+="
        onActivated: Settings.uiScale += 0.1
    }

    Shortcut {
        sequence: "Ctrl++"
        onActivated: Settings.uiScale += 0.1
    }

    Shortcut {
        sequence: "Ctrl+-"
        onActivated: Settings.uiScale -= 0.1
    }

    Shortcut {
        sequence: "F5"
        enabled: !settingsDialog.visible
        onActivated: NodeManager.engineRunning ? NodeManager.cancelRun() : NodeManager.run()
    }

    Shortcut {
        sequence: "Ctrl+0"
        enabled: !settingsDialog.visible
        onActivated: canvas.fitView()
    }

    Shortcut {
        sequence: "Escape"
        enabled: !settingsDialog.visible
        onActivated: NodeManager.mousePressEvent(Qt.point(-100000, -100000), false)
    }
}
