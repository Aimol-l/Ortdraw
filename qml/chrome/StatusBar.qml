import QtQuick
import QtQuick.Controls
import Theme
import NodeManager
import Settings

Rectangle {
    id: root

    height: 40
    color: Theme.bgPanel

    property int nodeCount: 0
    property int edgeCount: 0
    property string selectedName: "无"
    property string selectedPos: "-"
    property real zoom: 1.0

    readonly property bool queueMode: NodeManager.queueHasResult
    readonly property string themeName: Theme.dark ? "暗色" : "亮色"
    readonly property var zoomPresets: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 4.0]

    // 内部尺寸都按状态条高度等比推导，便于调整高度
    readonly property real chipH: Math.round(root.height * 0.6)
    readonly property real dotS: Math.round(root.height * 0.22)
    readonly property real runDotS: Math.round(root.height * 0.18)
    readonly property int fMain: Math.max(11, Math.round(root.height * 0.33))
    readonly property int fSmall: Math.max(10, Math.round(root.height * 0.28))
    readonly property real zoomH: Math.round(root.height * 0.6)
    readonly property real zoomInnerH: Math.round(root.height * 0.5)

    signal zoomInRequested()
    signal zoomOutRequested()
    signal zoomSetRequested(real z)
    signal fitRequested()

    function statusColor(s) {
        if (s === "ok") return Theme.success
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
        var ok = done - NodeManager.queueFailed - NodeManager.queueSkipped - NodeManager.queueCancelled
        var t = "完成 " + ok + "/" + total
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
            width: root.dotS
            height: root.dotS
            radius: root.dotS / 2
            color: NodeManager.engineStatus === "失败" ? Theme.red
                 : NodeManager.engineRunning ? Theme.yellow : Theme.success
        }

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            text: root.queueMode ? root.summaryText() : NodeManager.engineStatus
            color: NodeManager.engineStatus === "失败" ? Theme.red : Theme.fgDim
            font.pixelSize: root.fMain
        }
    }

    // 组数 ≥ 2 时出现：选择要查看的链
    Rectangle {
        id: groupSel
        visible: root.queueMode && NodeManager.queueGroups.length >= 2
        anchors.left: leftGroup.right
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: Math.round(root.height * 0.62)
        width: selRow.implicitWidth + 16
        radius: Math.round(root.height * 0.15)
        color: selHover.hovered || groupMenu.opened ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        function currentLabel() {
            if (NodeManager.selectedGroup < 0) return "全部组 (" + NodeManager.queueGroups.length + ")"
            var gs = NodeManager.queueGroups
            for (var i = 0; i < gs.length; ++i)
                if (gs[i].id === NodeManager.selectedGroup) return gs[i].name
            return "全部组 (" + gs.length + ")"
        }
        function currentColor() {
            if (NodeManager.selectedGroup < 0) return Theme.fgDim
            for (var i = 0; i < NodeManager.queueGroups.length; ++i)
                if (NodeManager.queueGroups[i].id === NodeManager.selectedGroup)
                    return NodeManager.queueGroups[i].color
            return Theme.fgDim
        }

        Row {
            id: selRow
            anchors.centerIn: parent
            spacing: 6
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(root.height * 0.2); height: width; radius: width / 2
                color: groupSel.currentColor()
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: groupSel.currentLabel()
                color: Theme.fg
                font.pixelSize: root.fSmall
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"; color: Theme.fgDim; font.pixelSize: root.fSmall
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }

        HoverHandler { id: selHover }
        TapHandler { onTapped: groupMenu.opened ? groupMenu.close() : groupMenu.open() }

        Menu {
            id: groupMenu
            y: -(height + 6)
            background: Rectangle {
                implicitWidth: 220
                color: Theme.bgElev
                border.width: 1
                border.color: Theme.border
                radius: 8
            }
            StMenuItem {
                text: "全部组 (" + NodeManager.queueGroups.length + ")"
                onTriggered: NodeManager.selectedGroup = -1
            }
            Instantiator {
                model: NodeManager.queueGroups
                delegate: StMenuItem {
                    required property var modelData
                    text: {
                        var s = (modelData.id === NodeManager.selectedGroup ? "● " : "") + modelData.name
                        var parts = []
                        if (modelData.failed > 0) parts.push("失败" + modelData.failed)
                        if (modelData.skipped > 0) parts.push("跳过" + modelData.skipped)
                        s += "  · " + modelData.count + " 节点"
                        if (parts.length) s += " · " + parts.join(" ")
                        return s
                    }
                    onTriggered: NodeManager.selectedGroup = modelData.id
                }
                onObjectAdded: (index, object) => groupMenu.insertItem(index + 2, object)
                onObjectRemoved: (index, object) => groupMenu.removeItem(object)
            }
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
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            text: "节点 " + root.nodeCount
            color: Theme.fgDim
            font.pixelSize: root.fMain
        }

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            text: "连线 " + root.edgeCount
            color: Theme.fgDim
            font.pixelSize: root.fMain
        }
    }

    // 运行过：中间常驻运行队列。
    ListView {
        id: queue
        visible: root.queueMode
        anchors.left: groupSel.visible ? groupSel.right : leftGroup.right
        anchors.leftMargin: 16
        anchors.right: rightGroup.left
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        height: root.chipH
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
                followAnim.stop()
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
            required property int group
            required property string groupColor
            required property bool groupStart

            // 该片是否为「组边界」（组间竖线），是则不画箭头
            readonly property bool showGroupSep: del.index > 0 && del.groupStart
                && NodeManager.queueGroups.length >= 2 && NodeManager.selectedGroup < 0

            height: queue.height
            width: layout.width

            Row {
                id: layout
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                // 「全部组」视图：组与组之间一条竖线（同组同色）
                Rectangle {
                    visible: del.showGroupSep
                    anchors.verticalCenter: parent.verticalCenter
                    width: 2
                    height: Math.round(root.chipH * 0.8)
                    radius: 1
                    color: del.groupColor
                }

                Text {
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    id: arrow
                    visible: del.index > 0 && !del.showGroupSep
                    anchors.verticalCenter: parent.verticalCenter
                    text: "→"
                    color: Theme.fgDim
                    font.pixelSize: root.fSmall
                }

                Rectangle {
                    id: chip
                    anchors.verticalCenter: parent.verticalCenter
                    height: root.chipH
                    width: implicitWidth
                    implicitWidth: chipRow.implicitWidth + 12
                    radius: Math.round(root.chipH * 0.28)
                    clip: true
                    color: del.status === "failed"
                           ? Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.3)
                           : del.status === "ok"
                               ? Qt.rgba(Theme.success.r, Theme.success.g, Theme.success.b, 1)
                             : del.status === "skipped"
                               ? Qt.rgba(Theme.yellow.r, Theme.yellow.g, Theme.yellow.b, 0.16)
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
                        from: Math.round(root.height * 0.45)
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
                            slide.x = Math.round(root.height * 0.45)
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
                            width: root.runDotS
                            height: root.runDotS
                            radius: root.runDotS / 2
                            color: Theme.blue
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.verticalCenter: parent.verticalCenter
                            visible: del.status !== "running"
                            text: root.statusGlyph(del.status)
                            color: root.statusColor(del.status)
                            font.pixelSize: root.fSmall
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.verticalCenter: parent.verticalCenter
                            text: del.name
                            color: Theme.fg
                            font.pixelSize: root.fMain
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.verticalCenter: parent.verticalCenter
                            text: del.ms > 0 ? del.ms + "ms" : ""
                            color: Theme.fgDim
                            font.pixelSize: root.fSmall
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

                    ToolTip {
                        id: errTip
                        text: del.error
                        visible: (del.status === "failed" || del.status === "skipped")
                                 && hoverArea.hovered && del.error !== ""
                        delay: 300
                        padding: 0
                        width: errTipText.implicitWidth + 16
                        height: errTipText.implicitHeight + 8
                        x: Math.round((chip.width - width) / 2)
                        y: chip.height + 4
                        background: Rectangle {
                            color: Theme.bgElev
                            border.width: 1
                            border.color: Theme.border
                            radius: 5
                        }
                        contentItem: Text {
                            id: errTipText
                            text: errTip.text
                            color: Theme.fg
                            font.pixelSize: root.fMain
                            renderType: Text.NativeRendering
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: NodeManager
        function onFocusQueueNode(uuid) {
            Qt.callLater(function() {
                var i = NodeManager.queueIndexOf(uuid)
                if (i >= 0) queue.positionViewAtIndex(i, ListView.Contain)
            })
        }
    }

    Row {
        id: rightGroup
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            text: "选中 " + root.selectedName + " · 坐标[" + root.selectedPos + "]"
            color: Theme.fgDim
            font.pixelSize: root.fMain
        }

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            visible: root.queueMode
            text: "节点 " + root.nodeCount + " · 连线 " + root.edgeCount
            color: Theme.fgDim
            font.pixelSize: root.fMain
        }

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.verticalCenter: parent.verticalCenter
            text: "主题 " + root.themeName
            color: Theme.fgDim
            font.pixelSize: root.fMain
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: zoomRow.width + 8
            height: root.zoomH
            radius: Math.round(root.height * 0.15)
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
                    height: root.zoomInnerH
                    radius: Math.round(root.height * 0.1)
                    color: zoomOutArea.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        anchors.centerIn: parent
                        text: "−"
                        color: Theme.fgDim
                        font.pixelSize: root.fMain
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
                    height: root.zoomInnerH
                    radius: Math.round(root.height * 0.1)
                    color: zoomInput.activeFocus ? Theme.bgPanel : "transparent"
                    border.width: zoomInput.activeFocus ? 1 : 0
                    border.color: Theme.blue

                    TextInput {
                        id: zoomInput
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: root.fMain
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
                    height: root.zoomInnerH
                    radius: Math.round(root.height * 0.1)
                    color: zoomMenuArea.containsMouse ? Theme.bgHover
                         : zoomMenu.opened ? Theme.bgHover : "transparent"

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        anchors.centerIn: parent
                        text: "▾"
                        color: Theme.fgDim
                        font.pixelSize: root.fSmall
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
                        background: Rectangle {
                            implicitWidth: 140
                            color: Theme.bgElev
                            border.width: 1
                            border.color: Theme.border
                            radius: 8
                        }
                        Instantiator {
                            model: root.zoomPresets
                            delegate: StMenuItem {
                                required property real modelData
                                text: Math.round(modelData * 100) + "%"
                                onTriggered: root.zoomSetRequested(modelData)
                            }
                            onObjectAdded: (index, object) => zoomMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => zoomMenu.removeItem(object)
                        }
                        StMenuSep {}
                        StMenuItem {
                            text: "适应视图"
                            onTriggered: root.fitRequested()
                        }
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20
                    height: root.zoomInnerH
                    radius: Math.round(root.height * 0.1)
                    color: zoomInArea.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        anchors.centerIn: parent
                        text: "+"
                        color: Theme.fgDim
                        font.pixelSize: root.fMain
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

    // 主题化菜单项/分隔线（暗色下也正确）
    component StMenuItem: MenuItem {
        id: stmi
        implicitHeight: 28
        contentItem: Text {
            text: stmi.text
            color: stmi.hovered ? Theme.fgBright : Theme.fg
            font.pixelSize: root.fSmall
            leftPadding: 12
            rightPadding: 12
            verticalAlignment: Text.AlignVCenter
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
        }
        background: Rectangle {
            radius: 6
            color: stmi.hovered ? Theme.bgHover : "transparent"
        }
    }
    component StMenuSep: MenuSeparator {
        implicitHeight: 9
        background: Rectangle {
            implicitHeight: 1
            color: Theme.border
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
