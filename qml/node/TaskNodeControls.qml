import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 任务节点通用控件（紧凑版）：
//   任务行 = 任务下拉（贴文本宽，clamp 80–132）+ 首个 bool 参数（无 bool 时为首个参数）
//   其余参数 = 两列网格（列距 10、行距 4）；固有宽度超单列的参数独占一行
Column {
    id: root

    property var node
    // 参数/任务变化时递增，用于强制重新求值（函数调用绑定不会自动刷新）
    property int revision: 0

    readonly property int gap: 10
    readonly property int ctlHeight: 20

    spacing: 4
    width: parent ? parent.width : implicitWidth

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    TextMetrics {
        id: taskMetrics
        font.pixelSize: 11
        text: (root.revision, root.taskLabel())
    }

    // 粗略文本宽：中文字宽≈字号，ASCII≈0.56 倍（用于跨列判断，无需精确）
    function textWidth(s, px) {
        if (!s) return 0
        var w = 0
        for (var i = 0; i < s.length; ++i)
            w += s.charCodeAt(i) > 255 ? px : px * 0.56
        return w
    }
    function groupOf(d) { return d.group === undefined ? 0 : d.group }
    // 两个参数能否拼成一行：都不跨列，且 group>0 的参数只与同 group 成行（不与 group 0 混排）
    function pairOK(a, b, cellW) {
        if (naturalWidth(a) > cellW || naturalWidth(b) > cellW) return false
        var ga = groupOf(a), gb = groupOf(b)
        if (ga > 0 || gb > 0) return ga > 0 && ga === gb
        return true
    }

    // 参数的固有宽度；-1 表示“标量，恒为单列”
    function naturalWidth(d) {
        if (!d) return -1
        var lw = textWidth(d.label, 10) + 10
        if (d.kind === "floats") {
            var n = d.vecCount ? d.vecCount : 1
            return lw + n * 40 + (n - 1) * 4
        }
        if (d.kind === "size2") return lw + 38 + 3 + 7 + 3 + 38   // 两格 38 + “×” + 间距
        if (d.kind === "file") return lw + 90 + 6 + 48             // 路径框 + 浏览按钮 → 跨列
        if (d.kind === "select") {
            // 用最长选项（而非当前值）估算，保证切换选项时布局稳定
            var maxOpt = 0
            var opts = d.options ? d.options : []
            for (var i = 0; i < opts.length; ++i) {
                var w = textWidth(opts[i].label, 11)
                if (w > maxOpt) maxOpt = w
            }
            return lw + maxOpt + 26
        }
        return -1
    }
    // 最上层校验（同 ParamField.claimClick）：被压住时不响应，改为选中并置顶最上层节点
    function claimClick(item, p) {
        if (!node || !node.parent || !item) return true
        var w = item.mapToItem(node.parent, p.x, p.y)
        var top = NodeManager.topNodeUuidAt(w.x, w.y)
        if (top === "" || top === ("" + node.uuid)) return true
        NodeManager.bringToFront(top)
        NodeManager.mousePressEvent(Qt.point(w.x, w.y), false)
        return false
    }
    function taskLabel() {
        if (!node) return ""
        var opts = node.taskOptions()
        for (var i = 0; i < opts.length; ++i)
            if (opts[i].id === node.task) return opts[i].name
        return node.task
    }

    // 计算：任务行参数 + 网格行（每行 cells: [{desc, w}]）
    readonly property var layout: {
        if (!node) return { task: null, rows: [] }
        root.revision
        var W = root.width
        var cellW = Math.max(0, (W - root.gap) / 2)

        var all = node.paramDescs()
        var cur = node.taskParams()
        var descs = []
        for (var i = 0; i < all.length; ++i) {
            var d = all[i]
            if (d.showIfKey !== undefined && d.showIfKey !== "" &&
                ("" + cur[d.showIfKey]) !== ("" + d.showIfValue)) continue
            descs.push(d)
        }

        // 任务行：优先第一个 bool；否则第一个参数
        var taskParam = null
        var rest = []
        var boolIdx = -1
        for (var b = 0; b < descs.length; ++b)
            if (descs[b].kind === "bool") { boolIdx = b; break }
        if (boolIdx >= 0) {
            taskParam = descs[boolIdx]
            for (var j = 0; j < descs.length; ++j) if (j !== boolIdx) rest.push(descs[j])
        } else if (descs.length > 0) {
            taskParam = descs[0]
            for (var k = 1; k < descs.length; ++k) rest.push(descs[k])
        }

        // 网格：可配对的两个参数成一行；跨列参数独占整行（不重排顺序）
        var rows = []
        var r = 0
        while (r < rest.length) {
            if (naturalWidth(rest[r]) > cellW) {
                rows.push({ cells: [{ desc: rest[r], w: W }] })
                ++r
                continue
            }
            if (r + 1 < rest.length && pairOK(rest[r], rest[r + 1], cellW)) {
                rows.push({ cells: [{ desc: rest[r], w: cellW },
                                    { desc: rest[r + 1], w: cellW }] })
                r += 2
                continue
            }
            rows.push({ cells: [{ desc: rest[r], w: cellW }] })
            ++r
        }
        return { task: taskParam, rows: rows }
    }

    Connections {
        target: root.node
        ignoreUnknownSignals: true
        function onParamsChanged() { root.revision++ }
        function onTaskPortsChanged() { root.revision++ }
    }

    // ---- 任务行 ----
    Row {
        id: taskRow
        width: root.width
        height: root.ctlHeight
        spacing: root.gap

        Rectangle {
            id: taskBox
            width: Math.max(80, Math.min(132, taskMetrics.advanceWidth + 30))
            height: root.ctlHeight
            radius: 5
            color: taskHover.hovered || taskMenu.visible ? Theme.bgHover : Theme.bg
            border.width: 1
            border.color: Theme.border

            Text {
                anchors.fill: parent
                anchors.leftMargin: 7
                anchors.rightMargin: 18
                verticalAlignment: Text.AlignVCenter
                text: root.node ? (root.revision, root.taskLabel()) : ""
                color: Theme.fg
                font.pixelSize: 11
                elide: Text.ElideRight
                renderType: root.textRenderType
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"
                color: Theme.fgDim
                font.pixelSize: 10
                renderType: root.textRenderType
            }

            HoverHandler { id: taskHover }
            TapHandler {
                onTapped: (p) => {
                    if (!root.claimClick(taskBox, p.position)) return
                    taskMenu.visible ? taskMenu.close() : taskMenu.open()
                }
            }

            Menu {
                id: taskMenu
                // 渲染到窗口 overlay 层，避免被节点内后续子项遮挡
                parent: Overlay.overlay
                onAboutToShow: {
                    var p = taskBox.mapToItem(null, 0, taskBox.height + 6)
                    x = p.x
                    y = p.y
                }
                background: Rectangle {
                    implicitWidth: 200
                    color: Theme.bgElev
                    border.width: 1
                    border.color: Theme.border
                    radius: 8
                }
                Instantiator {
                    model: root.node ? root.node.taskOptions() : []
                    delegate: MenuItem {
                        id: taskItem
                        required property var modelData
                        implicitHeight: 28
                        text: modelData.name
                        onTriggered: {
                            if (root.node) {
                                root.node.task = modelData.id
                                NodeManager.commitNodeParams(root.node.uuid)
                            }
                        }
                        contentItem: Text {
                            text: taskItem.text
                            color: taskItem.hovered ? Theme.fgBright : Theme.fg
                            font.pixelSize: 11
                            leftPadding: 12
                            rightPadding: 12
                            verticalAlignment: Text.AlignVCenter
                            renderType: root.textRenderType
                        }
                        background: Rectangle {
                            radius: 6
                            color: taskItem.hovered ? Theme.bgHover : "transparent"
                        }
                    }
                    onObjectAdded: (index, object) => taskMenu.insertItem(index, object)
                    onObjectRemoved: (index, object) => taskMenu.removeItem(object)
                }
            }
        }

        ParamField {
            visible: root.layout.task !== null
            node: root.node
            desc: root.layout.task
            revision: root.revision
            width: Math.max(0, taskRow.width - taskBox.width - root.gap)
        }
    }

    // ---- 两列参数网格 ----
    Repeater {
        model: root.layout.rows
        delegate: Row {
            id: gridRow
            required property var modelData
            width: root.width
            height: root.ctlHeight
            spacing: root.gap

            Repeater {
                model: gridRow.modelData.cells
                delegate: ParamField {
                    required property var modelData
                    node: root.node
                    desc: modelData.desc
                    revision: root.revision
                    width: modelData.w
                }
            }
        }
    }
}
