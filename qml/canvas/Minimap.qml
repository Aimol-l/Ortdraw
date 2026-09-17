import QtQuick
import Theme
import NodeManager
import Settings

Rectangle {
    id: mini
    width: 210; height: 140
    radius: 9
    color: Theme.bgPanel
    border.color: Theme.border
    border.width: 1
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    property real viewWidth: 1     // visible world width  (area.width / zoom)
    property real viewHeight: 1
    property var _map: ({ s: 1, ox: 0, oy: 0 })
    signal jumpTo(real wx, real wy)

    function jumpFrom(px, py) {
        const m = mini._map
        mini.jumpTo((px - m.ox) / m.s, (py - m.oy) / m.s)
    }

    function requestRepaint() {
        if (Settings.minimapFps > 0)
            throttle.restart()
        else
            cv.requestPaint()
    }

    Timer {
        id: throttle
        interval: Settings.minimapFps > 0 ? 1000 / Settings.minimapFps : 0
        repeat: false
        onTriggered: cv.requestPaint()
    }

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d"); ctx.reset()
            const W = width, H = height
            const nodes = NodeManager.nodeSnapshots()
            const edges = NodeManager.edgeSnapshots()
            const vx = -mini.panX / mini.zoom
            const vy = -mini.panY / mini.zoom
            const vw = mini.viewWidth, vh = mini.viewHeight
            let minX = vx, minY = vy, maxX = vx + vw, maxY = vy + vh
            for(const n of nodes){ minX = Math.min(minX, n.x); minY = Math.min(minY, n.y)
                                   maxX = Math.max(maxX, n.x + n.w); maxY = Math.max(maxY, n.y + n.h) }
            const pad = 80
            minX -= pad; minY -= pad; maxX += pad; maxY += pad
            const s = Math.min(W/(maxX-minX), H/(maxY-minY))
            const ox = (W - (maxX-minX)*s)/2 - minX*s
            const oy = (H - (maxY-minY)*s)/2 - minY*s
            mini._map = { s: s, ox: ox, oy: oy }
            // edges
            ctx.strokeStyle = Theme.wire; ctx.lineWidth = 1
            for(const e of edges){ ctx.beginPath()
                ctx.moveTo(e.fromX*s+ox, e.fromY*s+oy)
                ctx.lineTo(e.toX*s+ox, e.toY*s+oy); ctx.stroke() }
            // nodes
            for(const n of nodes){
                ctx.fillStyle = n.category === "input"  ? Theme.catInput
                              : n.category === "math"   ? Theme.catMath
                              : n.category === "output" ? Theme.catOutput
                              : Theme.catProcess
                ctx.fillRect(n.x*s+ox, n.y*s+oy, Math.max(2, n.w*s), Math.max(2, n.h*s))
            }
            // viewport
            ctx.strokeStyle = Theme.blue; ctx.lineWidth = 1.4
            ctx.strokeRect(vx*s+ox, vy*s+oy, vw*s, vh*s)
        }
    }
    MouseArea {
        anchors.fill: parent
        onPressed: (e) => mini.jumpFrom(e.x, e.y)
        onPositionChanged: (e) => { if (pressed) mini.jumpFrom(e.x, e.y) }
    }
    Connections { target: NodeManager; function onGraphChanged(){ mini.requestRepaint() } }
    Connections { target: Theme;     function onChanged(){ cv.requestPaint() } }
    onZoomChanged: mini.requestRepaint()
    onPanXChanged: mini.requestRepaint()
    onPanYChanged: mini.requestRepaint()
    onViewWidthChanged: mini.requestRepaint()
    onViewHeightChanged: mini.requestRepaint()
    Component.onCompleted: cv.requestPaint()
}
