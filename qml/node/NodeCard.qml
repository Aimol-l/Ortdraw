pragma ComponentBehavior: Bound
import QtQuick
import Theme
import NodeManager
import UiBus

Item {
    id: card

    property var node
    property var coordItem
    // 节点自定义内容（参数控件）注入点，位于端口行下方
    default property alias extraContent: extraHost.data

    readonly property color accent: node && node.category === "input"  ? Theme.catInput
                                  : node && node.category === "math"   ? Theme.catMath
                                  : node && node.category === "output" ? Theme.catOutput
                                  : Theme.catProcess

    readonly property bool selected: node ? node.selected : false

    readonly property int headHeight: 46
    readonly property int rowHeight: 30
    readonly property int bodyPadding: 8

    Rectangle {
        id: glow
        visible: card.selected
        z: -2
        anchors.centerIn: bg
        width: bg.width + 8
        height: bg.height + 8
        radius: 14
        color: "transparent"
        border.width: 1
        border.color: Theme.blue
        opacity: 0.45
    }

    // 轻量分层阴影：不使用 layer/MultiEffect，避免离屏纹理导致文字发虚
    Rectangle {
        z: -2
        x: 2
        y: 5
        width: card.width
        height: card.height
        radius: 12
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.20 : 0.055)
    }

    Rectangle {
        id: shadow
        z: -1
        x: 1
        y: 3
        width: card.width
        height: card.height
        radius: 11
        color: Qt.rgba(0, 0, 0, Theme.dark ? 0.30 : 0.08)
    }

    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 10
        color: Theme.bgElev
        border.width: card.selected ? 2 : 1
        border.color: card.selected ? Theme.blue : Theme.border

        Rectangle {
            id: head
            width: parent.width
            height: card.headHeight
            color: "transparent"
            topLeftRadius: 9
            topRightRadius: 9
            gradient: Gradient {
                GradientStop {
                    position: 0.0
                    color: Theme.dark ? Qt.rgba(1, 1, 1, 0.045)
                                      : Qt.rgba(20 / 255, 30 / 255, 70 / 255, 0.035)
                }
                GradientStop { position: 1.0; color: "transparent" }
            }

            Rectangle {
                anchors.centerIn: accentDot
                width: 16
                height: 16
                radius: 8
                color: card.accent
                opacity: 0.25
            }

            Rectangle {
                id: accentDot
                width: 8
                height: 8
                radius: 3
                color: card.accent
                anchors.left: parent.left
                anchors.leftMargin: 11
                anchors.verticalCenter: parent.verticalCenter
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 30
                anchors.right: kindText.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: card.node ? card.node.name : ""
                color: Theme.fgBright
                font.pixelSize: 14
                font.bold: true
                renderType: Text.CurveRendering
                elide: Text.ElideRight
            }

            Text {
                id: kindText
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                text: card.node ? card.node.typeName : ""
                color: Theme.fgDim
                font.family: "monospace"
                font.pixelSize: 11
                renderType: Text.CurveRendering
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.borderSoft
            }

            MouseArea {
                id: headArea
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                preventStealing: true
                cursorShape: headArea.dragging ? Qt.ClosedHandCursor
                             : (headArea.containsMouse ? Qt.OpenHandCursor : Qt.ArrowCursor)
                property point lastPos: Qt.point(0, 0)
                property real startNodeX: 0
                property real startNodeY: 0
                property bool dragging: false

                onPressed: (mouse) => {
                    headArea.dragging = (mouse.button === Qt.LeftButton)
                    if (headArea.dragging && card.coordItem) {
                        lastPos = mapToItem(card.coordItem, mouse.x, mouse.y)
                        startNodeX = card.node.x
                        startNodeY = card.node.y
                    }
                }
                onReleased: headArea.dragging = false
                onPositionChanged: (mouse) => {
                    if (!headArea.dragging || !card.node || !card.coordItem)
                        return
                    var p = mapToItem(card.coordItem, mouse.x, mouse.y)
                    var nx = startNodeX + (p.x - lastPos.x)
                    var ny = startNodeY + (p.y - lastPos.y)
                    if (UiBus.snapEnabled) {
                        nx = Math.round(nx / 8) * 8
                        ny = Math.round(ny / 8) * 8
                    }
                    var adx = nx - card.node.x
                    var ady = ny - card.node.y
                    if (adx !== 0 || ady !== 0) {
                        card.node.x = nx
                        card.node.y = ny
                        NodeManager.nodeMoveEvent(card.node.uuid, adx, ady)
                    }
                }
                onClicked: (mouse) => {
                    if (!card.node)
                        return
                    if (mouse.button === Qt.RightButton) {
                        NodeManager.clickNodeEvent(card.node.uuid, false)
                        var g = card.mapToItem(null, mouse.x, mouse.y)
                        UiBus.contextMenuRequested(g.x, g.y, "node", { uid: card.node.uuid })
                        return
                    }
                    NodeManager.clickNodeEvent(card.node.uuid,
                        (mouse.modifiers & Qt.ControlModifier) !== 0)
                }
            }
        }

        Column {
            id: rows
            anchors.top: head.bottom
            anchors.topMargin: card.bodyPadding
            anchors.left: parent.left
            anchors.right: parent.right

            Repeater {
                model: card.node ? Math.max(card.node.inputPorts.length, card.node.outputPorts.length) : 0

                delegate: Item {
                    id: rowItem
                    required property int index
                    width: rows.width
                    height: card.rowHeight
                    readonly property var inPort: (card.node && index < card.node.inputPorts.length)
                                                  ? card.node.inputPorts[index] : null
                    readonly property var outPort: (card.node && index < card.node.outputPorts.length)
                                                   ? card.node.outputPorts[index] : null

                    // ---- 输入（左） ----
                    Rectangle {
                        id: inDot
                        visible: rowItem.inPort !== null
                        x: -7
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.portIn
                        border.width: 2
                        border.color: Theme.bgElev
                        scale: inArea.containsMouse ? 1.45 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120 } }

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 8
                            height: parent.height + 8
                            radius: width / 2
                            color: Theme.portIn
                            opacity: inArea.containsMouse ? 0.3 : 0.0
                            z: -1
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }

                        MouseArea {
                            id: inArea
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.CrossCursor
                            onClicked: {
                                if (rowItem.inPort && card.coordItem) {
                                    var p = inDot.mapToItem(card.coordItem, inDot.width / 2, inDot.height / 2)
                                    NodeManager.setInputPort(rowItem.inPort.self, p.x, p.y)
                                }
                            }
                        }

                        Component.onCompleted: {
                            if (rowItem.inPort && card.coordItem) {
                                var p = inDot.mapToItem(card.coordItem, inDot.width / 2, inDot.height / 2)
                                card.node.setInputPortPosition(rowItem.index, p.x, p.y)
                            }
                        }
                    }

                    Row {
                        visible: rowItem.inPort !== null
                        anchors.left: parent.left
                        anchors.leftMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.inPort ? rowItem.inPort.name : ""
                            color: Theme.fg
                            font.pixelSize: 11
                            renderType: Text.CurveRendering
                        }

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 16
                            width: inTag.implicitWidth + 10
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.borderSoft
                            radius: 4

                            Text {
                                id: inTag
                                anchors.centerIn: parent
                                text: rowItem.inPort ? rowItem.inPort.dataTypeName : ""
                                color: Theme.fgDim
                                font.family: "monospace"
                                font.pixelSize: 9
                                renderType: Text.CurveRendering
                            }
                        }
                    }

                    // ---- 输出（右，与输入同一行） ----
                    Row {
                        visible: rowItem.outPort !== null
                        anchors.right: parent.right
                        anchors.rightMargin: 20
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 6

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            height: 16
                            width: outTag.implicitWidth + 10
                            color: Theme.bg
                            border.width: 1
                            border.color: Theme.borderSoft
                            radius: 4

                            Text {
                                id: outTag
                                anchors.centerIn: parent
                                text: rowItem.outPort ? rowItem.outPort.dataTypeName : ""
                                color: Theme.fgDim
                                font.family: "monospace"
                                font.pixelSize: 9
                                renderType: Text.CurveRendering
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: rowItem.outPort ? rowItem.outPort.name : ""
                            color: Theme.fg
                            font.pixelSize: 11
                            renderType: Text.CurveRendering
                        }
                    }

                    Rectangle {
                        id: outDot
                        visible: rowItem.outPort !== null
                        x: rowItem.width - 5
                        anchors.verticalCenter: parent.verticalCenter
                        width: 12
                        height: 12
                        radius: 6
                        color: Theme.portOut
                        border.width: 2
                        border.color: Theme.bgElev
                        scale: outArea.containsMouse ? 1.45 : 1.0
                        Behavior on scale { NumberAnimation { duration: 120 } }

                        Rectangle {
                            anchors.centerIn: parent
                            width: parent.width + 8
                            height: parent.height + 8
                            radius: width / 2
                            color: Theme.portOut
                            opacity: outArea.containsMouse ? 0.3 : 0.0
                            z: -1
                            Behavior on opacity { NumberAnimation { duration: 120 } }
                        }

                        MouseArea {
                            id: outArea
                            anchors.fill: parent
                            anchors.margins: -6
                            hoverEnabled: true
                            cursorShape: Qt.CrossCursor
                            onClicked: {
                                if (rowItem.outPort && card.coordItem) {
                                    var p = outDot.mapToItem(card.coordItem, outDot.width / 2, outDot.height / 2)
                                    NodeManager.setOutputPort(rowItem.outPort.self, p.x, p.y)
                                }
                            }
                        }

                        Component.onCompleted: {
                            if (rowItem.outPort && card.coordItem) {
                                var p = outDot.mapToItem(card.coordItem, outDot.width / 2, outDot.height / 2)
                                card.node.setOutputPortPosition(rowItem.index, p.x, p.y)
                            }
                        }
                    }
                }
            }
        }

        Item {
            id: extraHost
            anchors.top: rows.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 12
            anchors.rightMargin: 12
            anchors.topMargin: 8
            height: childrenRect.height
        }

        MouseArea {
            id: resizeArea
            width: 20
            height: 20
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            preventStealing: true
            property point lastPos: Qt.point(0, 0)

            onPressed: (mouse) => {
                if (card.coordItem)
                    lastPos = mapToItem(card.coordItem, mouse.x, mouse.y)
            }
            onPositionChanged: (mouse) => {
                if (!pressed || !card.node || !card.coordItem)
                    return
                var p = mapToItem(card.coordItem, mouse.x, mouse.y)
                var dx = p.x - lastPos.x
                var dy = p.y - lastPos.y
                lastPos = p
                var oldW = card.node.width
                var newW = Math.max(card.node.getMinWidth(), oldW + dx)
                var newH = Math.max(card.node.getMinHeight(), card.node.height + dy)
                var dw = newW - oldW
                card.node.width = newW
                card.node.height = newH
                NodeManager.nodeResizeEvent(card.node.uuid, dw, 0)
            }
        }
    }
}
