import QtQuick
import QtQuick.Window
import QtQuick.Controls as Controls
import Theme
import NodeManager

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

    property real leftPanelWidth: leftCollapsed ? 0 : 250
    property real rightPanelWidth: rightCollapsed ? 0 : 286

    Behavior on leftPanelWidth { NumberAnimation { duration: 200 } }
    Behavior on rightPanelWidth { NumberAnimation { duration: 200 } }

    // Dev aid (not a product feature): when launched with --demo, create two
    // sample nodes so the UI can be verified/screenshotted without a mouse.
    function demoPopulate() {
        palette.addNodeAt("ImageLoad", 80, 120)
        palette.addNodeAt("ImageShow", 720, 200)
    }

    Component.onCompleted: {
        leftCollapsed = width < 960
        rightCollapsed = width < 1180
        if (Qt.application.arguments.indexOf("--demo") >= 0)
            Qt.callLater(demoPopulate)
    }

    onWidthChanged: {
        leftCollapsed = width < 960
        rightCollapsed = width < 1180
    }

    Column {
        anchors.fill: parent

        TopBar {
            id: topbar
            width: parent.width
            gridVisible: canvas.gridVisible
            onUndoRequested: NodeManager.undo()
            onRedoRequested: NodeManager.redo()
            onFitRequested: canvas.fitView()
            onClearRequested: NodeManager.clearGraph()
            onGridToggled: canvas.gridVisible = !canvas.gridVisible
        }

        Item {
            id: mid
            width: parent.width
            height: win.height - topbar.height - statusbar.height
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

    ContextMenu {
        id: ctx
        nodeLayer: canvas.nodeLayer
        onFitRequested: canvas.fitView()
    }

    Shortcut {
        sequence: "Delete"
        onActivated: {
            NodeManager.removeNode()
            NodeManager.removeEdge()
        }
    }

    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: NodeManager.undo()
    }

    Shortcut {
        sequence: "Ctrl+Y"
        onActivated: NodeManager.redo()
    }

    Shortcut {
        sequence: "Ctrl+D"
        enabled: NodeManager.selectedNode !== null
        onActivated: ctx.cloneNode(NodeManager.selectedNode.uuid)
    }

    Shortcut {
        sequence: "Ctrl+0"
        onActivated: canvas.fitView()
    }

    Shortcut {
        sequence: "Escape"
        onActivated: NodeManager.mousePressEvent(Qt.point(-100000, -100000), false)
    }
}
