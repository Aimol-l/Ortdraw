import QtQuick
import QtQuick.Controls
import Theme
import NodeManager
import Settings

Rectangle {
    id: root

    height: 30
    color: Theme.bgPanel

    property int nodeCount: 0
    property int edgeCount: 0
    property string selectedName: "无"
    property string selectedPos: "-"
    property real zoom: 1.0

    readonly property bool queueMode: NodeManager.queueHasResult
    readonly property string themeName: Theme.dark ? "暗色" : "亮色"
    readonly property var zoomPresets: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 4.0]

    signal zoomInRequested()
    signal zoomOutRequested()
    signal zoomSetRequested(real z)
    signal fitRequested()

    function statusColor(s) {
        if (s === "ok") return Theme.green
        if (s === "failed") return Theme.red
        if (s === "skipped") return Theme.yellow
        if (s === "cancelled") return Theme.fgDim
        return Theme.blue
    }
    function statusGlyph(s) {
        if (s === "ok") return "✓"
        if (s === "failed") return "✕"
        if (s === "skipped") return "–"
        if (s === "cancelled") return "⊘"
        return ""
    }
    function summaryText() {
        var done = NodeManager.queueDone, total = NodeManager.queueTotal
        if (NodeManager.engineRunning) return "运行中… " + done + "/" + total
        if (NodeManager.queueCancelled > 0) return "已取消 " + done + "/" + total
        var t = "完成 " + done + "/" + total
        if (NodeManager.queueFailed > 0) t += " · 失败" + NodeManager.queueFailed
        if (NodeManager.queueSkipped > 0) t += " · 跳过" + NodeManager.queueSkipped
        return t
    }

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        id: leftGroup
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 6

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 7
            height: 7
            radius: 3.5
            color: NodeManager.engineStatus === "失败" ? Theme.red
                 : NodeManager.engineRunning ? Theme.yellow : Theme.green
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.queueMode ? root.summaryText() : NodeManager.engineStatus
            color: NodeManager.engineStatus === "失败" ? Theme.red : Theme.fgDim
            font.pixelSize: 11
        }
    }

    // 未运行过：中间显示「节点 / 连线」统计（坐标在右侧）。
    Row {
        id: statsGroup
        visible: !root.queueMode
        anchors.left: leftGroup.right
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "节点 " + root.nodeCount
            color: Theme.fgDim
            font.pixelSize: 11
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "连线 " + root.edgeCount
            color: Theme.fgDim
            font.pixelSize: 11
        }
    }

    // 运行过：中间常驻运行队列。
    ListView {
        id: queue
        visible: root.queueMode
        anchors.left: leftGroup.right
        anchors.leftMargin: 16
        anchors.right: rightGroup.left
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        height: 20
        orientation: ListView.Horizontal
        spacing: 4
        clip: true
        model: NodeManager.execQueue

        property bool userFollowPaused: false

        function scrollToEnd() {
            if (userFollowPaused) return
            var target = Math.max(0, contentWidth - width)
            if (Settings.queueAnimation) {
                followAnim.to = target
                followAnim.start()
            } else {
                positionViewAtEnd()
            }
        }

        onCountChanged: {
            userFollowPaused = false
            Qt.callLater(scrollToEnd)
        }

        NumberAnimation {
            id: followAnim
            target: queue
            property: "contentX"
            duration: 260
            easing.type: Easing.OutCubic
        }

        WheelHandler {
            onWheel: (event) => {
                queue.userFollowPaused = true
                var maxX = Math.max(0, queue.contentWidth - queue.width)
                queue.contentX = Math.max(0, Math.min(maxX, queue.contentX - event.angleDelta.y * 0.5))
            }
        }

        delegate: Item {
            id: del
            required property int index
            required property string uuid
            required property string name
            required property string status
            required property int ms
            required property string error

            height: queue.height
            width: layout.width

            Row {
                id: layout
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Text {
                    id: arrow
                    visible: del.index > 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: "→"
                    color: Theme.fgDim
                    font.pixelSize: 10
                }

                Rectangle {
                    id: chip
                    anchors.verticalCenter: parent.verticalCenter
                    height: 18
                    width: implicitWidth
                    implicitWidth: chipRow.implicitWidth + 12
                    radius: 5
                    color: del.status === "failed"
                           ? Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.15)
                           : Theme.bg
                    border.width: 1
                    border.color: root.statusColor(del.status)
                    opacity: 1

                    transform: Translate {
                        id: slide
                        x: 0
                    }

                    Behavior on border.color {
                        enabled: Settings.queueAnimation
                        ColorAnimation { duration: 250 }
                    }

                    // 运行中：边框脉冲（无底部扫光）。
                    SequentialAnimation {
                        id: pulse
                        loops: Animation.Infinite
                        running: Settings.queueAnimation && del.status === "running"
                        onStopped: chip.border.width = 1
                        NumberAnimation { target: chip; property: "border.width"; from: 1; to: 3; duration: 600 }
                        NumberAnimation { target: chip; property: "border.width"; from: 3; to: 1; duration: 600 }
                    }

                    NumberAnimation {
                        id: slideAnim
                        target: slide
                        property: "x"
                        from: 18
                        to: 0
                        duration: 320
                        easing.type: Easing.OutCubic
                    }

                    NumberAnimation {
                        id: fadeAnim
                        target: chip
                        property: "opacity"
                        from: 0
                        to: 1
                        duration: 240
                    }

                    Component.onCompleted: {
                        if (Settings.queueAnimation) {
                            chip.opacity = 0
                            slide.x = 18
                            slideAnim.start()
                            fadeAnim.start()
                        }
                    }

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: 4

                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: del.status === "running"
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.blue
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: del.status !== "running"
                            text: root.statusGlyph(del.status)
                            color: root.statusColor(del.status)
                            font.pixelSize: 10
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: del.name
                            color: Theme.fg
                            font.pixelSize: 11
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: del.ms > 0 ? del.ms + "ms" : ""
                            color: Theme.fgDim
                            font.pixelSize: 10
                            opacity: del.ms > 0 ? 1 : 0
                            Behavior on opacity {
                                enabled: Settings.queueAnimation
                                NumberAnimation { duration: 250 }
                            }
                        }
                    }

                    HoverHandler {
                        id: hoverArea
                        cursorShape: Qt.PointingHandCursor
                    }

                    TapHandler {
                        onTapped: NodeManager.focusNode(del.uuid)
                    }

                    ToolTip.visible: (del.status === "failed" || del.status === "skipped")
                                     && hoverArea.hovered && del.error !== ""
                    ToolTip.text: del.error
                    ToolTip.delay: 300
                }
            }
        }
    }

    Row {
        id: rightGroup
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "选中 " + root.selectedName + " · 坐标[" + root.selectedPos + "]"
            color: Theme.fgDim
            font.pixelSize: 11
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.queueMode
            text: "节点 " + root.nodeCount + " · 连线 " + root.edgeCount
            color: Theme.fgDim
            font.pixelSize: 11
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "主题 " + root.themeName
            color: Theme.fgDim
            font.pixelSize: 11
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: zoomRow.width + 8
            height: 22
            radius: 6
            color: Theme.bg
            border.width: 1
            border.color: Theme.border

            Row {
                id: zoomRow
                anchors.centerIn: parent
                spacing: 2

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: 18
                    radius: 4
                    color: zoomOutArea.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "−"
                        color: Theme.fgDim
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: zoomOutArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.zoomOutRequested()
                    }
                }

                // 可编辑的缩放百分比
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 46
                    height: 18
                    radius: 4
                    color: zoomInput.activeFocus ? Theme.bgPanel : "transparent"
                    border.width: zoomInput.activeFocus ? 1 : 0
                    border.color: Theme.blue

                    TextInput {
                        id: zoomInput
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        selectionColor: Theme.blue
                        selectedTextColor: "#ffffff"
                        cursorVisible: activeFocus

                        function refresh() { text = Math.round(root.zoom * 100) + "%" }
                        function commit() {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = Math.round(root.zoom * 100)
                            v = Math.min(240, Math.max(35, v))
                            root.zoomSetRequested(v / 100)
                            refresh()
                        }

                        onAccepted: commit()
                        onEditingFinished: commit()
                        onActiveFocusChanged: if (!activeFocus) refresh()

                        Connections {
                            target: root
                            function onZoomChanged() { if (!zoomInput.activeFocus) zoomInput.refresh() }
                        }
                        Component.onCompleted: refresh()
                    }
                }

                // 预设比例下拉
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 18
                    height: 18
                    radius: 4
                    color: zoomMenuArea.containsMouse ? Theme.bgHover
                         : zoomMenu.opened ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "▾"
                        color: Theme.fgDim
                        font.pixelSize: 10
                    }

                    MouseArea {
                        id: zoomMenuArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: zoomMenu.opened ? zoomMenu.close() : zoomMenu.open()
                    }

                    Menu {
                        id: zoomMenu
                        y: -(height + 4)
                        Instantiator {
                            model: root.zoomPresets
                            delegate: MenuItem {
                                required property real modelData
                                text: Math.round(modelData * 100) + "%"
                                onTriggered: root.zoomSetRequested(modelData)
                            }
                            onObjectAdded: (index, object) => zoomMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => zoomMenu.removeItem(object)
                        }
                        MenuSeparator {}
                        MenuItem {
                            text: "适应视图"
                            onTriggered: root.fitRequested()
                        }
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: 18
                    radius: 4
                    color: zoomInArea.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        anchors.centerIn: parent
                        text: "+"
                        color: Theme.fgDim
                        font.pixelSize: 12
                    }

                    MouseArea {
                        id: zoomInArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.zoomInRequested()
                    }
                }
            }
        }
    }
}
