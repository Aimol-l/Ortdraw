import QtQuick
import Theme
import NodeCatalog

Item {
    id: item

    property var entry
    property bool gridMode: false

    signal addRequested(string type)

    readonly property bool available: entry ? NodeCatalog.componentUrl(entry.type) !== "" : false

    opacity: available ? 1.0 : 0.45

    readonly property color catColor: !entry ? Theme.fg
                                      : entry.cat === "input"  ? Theme.catInput
                                      : entry.cat === "math"   ? Theme.catMath
                                      : entry.cat === "output" ? Theme.catOutput
                                      : Theme.catProcess

    implicitHeight: gridMode ? 88 : 46

    function paintIcon(ctx, name, size, color) {
        var k = size / 24
        ctx.resetTransform ? ctx.resetTransform() : ctx.setTransform(1, 0, 0, 1, 0, 0)
        ctx.clearRect(0, 0, size, size)
        ctx.save()
        ctx.scale(k, k)
        ctx.lineWidth = 1.7
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.strokeStyle = color
        ctx.fillStyle = color

        function rr(x, y, w, h, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.arcTo(x + w, y, x + w, y + h, r)
            ctx.arcTo(x + w, y + h, x, y + h, r)
            ctx.arcTo(x, y + h, x, y, r)
            ctx.arcTo(x, y, x + w, y, r)
            ctx.closePath()
        }
        function line(x1, y1, x2, y2) {
            ctx.beginPath()
            ctx.moveTo(x1, y1)
            ctx.lineTo(x2, y2)
            ctx.stroke()
        }

        if (name === "image") {
            rr(3, 4, 18, 15, 2.5); ctx.stroke()
            ctx.beginPath(); ctx.arc(8.5, 9, 1.6, 0, Math.PI * 2); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(4, 17); ctx.lineTo(9, 12); ctx.lineTo(13, 16)
            ctx.lineTo(16, 13); ctx.lineTo(20, 17); ctx.stroke()
        } else if (name === "monitor") {
            rr(3, 4, 18, 12.5, 2.5); ctx.stroke()
            line(9, 20.5, 15, 20.5)
            line(12, 16.5, 12, 20.5)
        } else if (name === "resize") {
            ctx.beginPath(); ctx.moveTo(4, 9); ctx.lineTo(4, 5.5)
            ctx.arcTo(4, 4, 5.5, 4, 1.5); ctx.lineTo(9, 4); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(20, 15); ctx.lineTo(20, 18.5)
            ctx.arcTo(20, 20, 18.5, 20, 1.5); ctx.lineTo(15, 20); ctx.stroke()
            line(4, 4, 10.5, 10.5)
            line(20, 20, 13.5, 13.5)
        } else if (name === "blur") {
            ctx.beginPath(); ctx.arc(12, 10.5, 5.5, 0, Math.PI * 2); ctx.stroke()
            line(12, 16, 12, 18.5)
            line(7.5, 20.5, 16.5, 20.5)
        } else if (name === "threshold") {
            ctx.beginPath(); ctx.arc(12, 12, 8, 0, Math.PI * 2); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(12, 4)
            ctx.arc(12, 12, 8, -Math.PI / 2, Math.PI / 2, false)
            ctx.closePath(); ctx.fill()
        } else if (name === "gray") {
            rr(3, 3, 18, 18, 3); ctx.stroke()
            ctx.beginPath(); ctx.arc(12, 12, 5.5, Math.PI / 2, -Math.PI / 2, false)
            ctx.closePath(); ctx.fill()
            ctx.beginPath(); ctx.arc(12, 12, 5.5, 0, Math.PI * 2); ctx.stroke()
        } else if (name === "edge") {
            rr(3, 4, 18, 16, 2.5); ctx.stroke()
            ctx.beginPath(); ctx.moveTo(5, 16); ctx.lineTo(9, 11)
            ctx.lineTo(12.5, 14); ctx.lineTo(19, 7); ctx.stroke()
        } else if (name === "conv") {
            rr(4, 4, 16, 16, 2.5); ctx.stroke()
            line(9.3, 4, 9.3, 20)
            line(14.7, 4, 14.7, 20)
            line(4, 9.3, 20, 9.3)
            line(4, 14.7, 20, 14.7)
        } else {
            ctx.beginPath()
            ctx.moveTo(12, 4.5); ctx.lineTo(19.5, 12); ctx.lineTo(12, 19.5); ctx.lineTo(4.5, 12)
            ctx.closePath(); ctx.stroke()
        }
        ctx.restore()
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 9
        color: item.gridMode ? Theme.bg
                             : (hover.containsMouse ? Theme.bgHover : "transparent")
        border.width: item.gridMode ? 1 : (hover.containsMouse ? 1 : 0)
        border.color: item.gridMode ? Theme.borderSoft : Theme.border
    }

    MouseArea {
        id: hover
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: item.available ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        onClicked: if (item.available) item.addRequested(item.entry ? item.entry.type : "")
    }

    Rectangle {
        id: icoBox
        width: 30
        height: 30
        radius: 8
        color: Qt.rgba(item.catColor.r, item.catColor.g, item.catColor.b, 0.14)
        x: item.gridMode ? (item.width - width) / 2 : 8
        y: item.gridMode ? 12 : (item.height - height) / 2

        Canvas {
            id: iconCanvas
            anchors.centerIn: parent
            width: 18
            height: 18
            onPaint: paintIcon(getContext("2d"), item.entry ? item.entry.icon : "", width, item.catColor)
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()

            Connections {
                target: item
                function onEntryChanged() { iconCanvas.requestPaint() }
                function onCatColorChanged() { iconCanvas.requestPaint() }
                function onGridModeChanged() { iconCanvas.requestPaint() }
            }
            Connections {
                target: Theme
                function onChanged() { iconCanvas.requestPaint() }
            }
            Component.onCompleted: requestPaint()
        }
    }

    Column {
        id: meta
        spacing: 1
        x: item.gridMode ? 6 : icoBox.x + icoBox.width + 10
        width: item.gridMode ? item.width - 12 : item.width - x - 34
        y: item.gridMode ? icoBox.y + icoBox.height + 7 : (item.height - height) / 2

        Text {
            width: parent.width
            horizontalAlignment: item.gridMode ? Text.AlignHCenter : Text.AlignLeft
            text: item.entry ? item.entry.title : ""
            color: Theme.fgBright
            font.pixelSize: 13
            font.bold: true
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            horizontalAlignment: item.gridMode ? Text.AlignHCenter : Text.AlignLeft
            text: item.available ? (item.entry ? item.entry.desc : "") : "尚未实现"
            color: Theme.fgDim
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }

    Rectangle {
        id: addBtn
        width: 22
        height: 22
        radius: 6
        color: addArea.pressed ? Theme.bgHover : "transparent"
        border.width: 1
        border.color: addArea.pressed ? Theme.blue : Theme.border
        opacity: hover.containsMouse ? 1.0 : 0.0
        visible: item.available && opacity > 0
        x: item.gridMode ? item.width - width - 6 : item.width - width - 8
        y: item.gridMode ? 6 : (item.height - height) / 2

        Behavior on opacity { NumberAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            text: "＋"
            color: addArea.pressed ? Theme.blue : Theme.fgDim
            font.pixelSize: 13
        }

        MouseArea {
            id: addArea
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: item.addRequested(item.entry ? item.entry.type : "")
        }
    }
}
