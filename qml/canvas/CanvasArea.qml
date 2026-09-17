import QtQuick
import PaintBoard
import NodeManager
import Theme
import UiBus
import Settings

Item {
    id: area
    focus: true

    property alias paintBoard: board
    property alias nodeLayer: nodes
    property real zoom: 1.0
    property real panX: 0
    property real panY: 0

    Keys.onPressed: (e) => {
        if (e.key === Qt.Key_Space && !e.isAutoRepeat) {
            UiBus.spaceHeld = true
            e.accepted = true
        }
    }

    Keys.onReleased: (e) => {
        if (e.key === Qt.Key_Space && !e.isAutoRepeat) {
            UiBus.spaceHeld = false
            e.accepted = true
        }
    }

    Connections {
        target: area.Window.window
        function onActiveChanged() {
            if (!area.Window.window.active)
                UiBus.spaceHeld = false
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Canvas {
        id: grid
        anchors.fill: parent
        visible: Settings.backgroundMode !== "none"

        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            var mode = Settings.backgroundMode
            if (mode === "none")
                return
            var spacing = Settings.gridSpacing
            var ox = ((area.panX % spacing) + spacing) % spacing
            var oy = ((area.panY % spacing) + spacing) % spacing
            if (mode === "dots") {
                ctx.fillStyle = Theme.grid
                for (var x = ox; x < width; x += spacing) {
                    for (var y = oy; y < height; y += spacing) {
                        ctx.beginPath()
                        ctx.arc(x, y, 1.2, 0, Math.PI * 2)
                        ctx.fill()
                    }
                }
            } else { // grid lines
                ctx.strokeStyle = Theme.grid
                ctx.lineWidth = 1
                for (var gx = ox; gx < width; gx += spacing) {
                    ctx.beginPath(); ctx.moveTo(gx, 0); ctx.lineTo(gx, height); ctx.stroke()
                }
                for (var gy = oy; gy < height; gy += spacing) {
                    ctx.beginPath(); ctx.moveTo(0, gy); ctx.lineTo(width, gy); ctx.stroke()
                }
            }
        }

        Connections {
            target: Theme
            function onChanged() { grid.requestPaint() }
        }

        Connections {
            target: Settings
            function onBackgroundModeChanged() { grid.requestPaint() }
            function onGridSpacingChanged() { grid.requestPaint() }
        }

        Connections {
            target: area
            function onPanXChanged() { grid.requestPaint() }
            function onPanYChanged() { grid.requestPaint() }
        }

        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
    }

    MouseArea {
        id: input
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: true
        cursorShape: input.panMode ? (input.panning ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor

        property bool panMode: UiBus.spaceHeld || !Settings.spaceToPan
        property bool panning: false
        property real lastX: 0
        property real lastY: 0

        onPressed: (mouse) => {
            area.forceActiveFocus()
            var p = area.toWorld(mouse.x, mouse.y)
            NodeManager.mousePressEvent(Qt.point(p.x, p.y), false)
            if (mouse.button === Qt.RightButton) {
                if (Settings.contextMenu) {
                    var kind = "canvas"
                    if (NodeManager.selectedNode)
                        kind = "node"
                    else if (NodeManager.selectedEdge.from !== undefined)
                        kind = "edge"
                    var g = area.mapToItem(null, mouse.x, mouse.y)
                    UiBus.contextMenuRequested(g.x, g.y, kind, {})
                }
                return
            }
            if (mouse.button === Qt.LeftButton && input.panMode) {
                panning = true
                lastX = mouse.x
                lastY = mouse.y
            }
        }

        onPositionChanged: (mouse) => {
            if (panning) {
                area.panX += mouse.x - lastX
                area.panY += mouse.y - lastY
                lastX = mouse.x
                lastY = mouse.y
            }
            var p = area.toWorld(mouse.x, mouse.y)
            NodeManager.mouseMoveEvent(p.x, p.y)
        }

        onReleased: panning = false

        onWheel: (wheel) => {
            var old = area.zoom
            var factor = wheel.angleDelta.y > 0 ? 1.1 : 1 / 1.1
            var nz = Math.min(Settings.zoomMax, Math.max(Settings.zoomMin, old * factor))
            if (nz !== old)
                area.applyZoom(nz, wheel.x, wheel.y)
            wheel.accepted = true
        }
    }

    // 连线画板：与视口同尺寸，内部按 pan/zoom 变换绘制世界坐标
    PaintBoard {
        id: board
        anchors.fill: parent
        zoom: area.zoom
        panX: area.panX
        panY: area.panY
    }

    Item {
        id: world

        transform: [
            Scale { origin.x: 0; origin.y: 0; xScale: area.zoom; yScale: area.zoom },
            Translate { x: area.panX; y: area.panY }
        ]

        Item {
            id: nodes
            width: 8000
            height: 8000
        }
    }

    Minimap {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        zoom: area.zoom
        panX: area.panX
        panY: area.panY
        viewWidth: area.width / area.zoom
        viewHeight: area.height / area.zoom
        onJumpTo: (wx, wy) => {
            area.panX = area.width/2  - wx * area.zoom
            area.panY = area.height/2 - wy * area.zoom
        }
    }

    function toWorld(x, y) {
        return area.mapToItem(world, x, y)
    }

    function applyZoom(nz, px, py) {
        nz = Math.min(Settings.zoomMax, Math.max(Settings.zoomMin, nz))
        var old = area.zoom
        if (nz === old)
            return
        area.panX = px - (px - area.panX) * (nz / old)
        area.panY = py - (py - area.panY) * (nz / old)
        area.zoom = nz
    }

    function zoomIn() {
        area.applyZoom(area.zoom * 1.15, area.width / 2, area.height / 2)
    }

    function zoomOut() {
        area.applyZoom(area.zoom / 1.15, area.width / 2, area.height / 2)
    }

    function setZoom(z) {
        area.applyZoom(z, area.width / 2, area.height / 2)
    }

    function fitView() {
        const r = nodes.childrenRect
        if (r.width <= 0 || r.height <= 0) {
            area.zoom = 1
            area.panX = 0
            area.panY = 0
            return
        }
        const m = Settings.fitMargin
        const z = Math.min(Settings.zoomMax, Math.max(Settings.zoomMin, Math.min((width - 2 * m) / r.width, (height - 2 * m) / r.height)))
        area.zoom = z
        area.panX = width / 2 - (r.x + r.width / 2) * z
        area.panY = height / 2 - (r.y + r.height / 2) * z
    }

    Connections {
        target: Settings
        function onSnapToGridChanged() { UiBus.snapEnabled = Settings.snapToGrid }
    }

    Component.onCompleted: {
        UiBus.snapEnabled = Settings.snapToGrid
        NodeManager.setPaintBoard(board)
    }
}
