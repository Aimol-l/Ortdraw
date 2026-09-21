import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 任务节点通用控件：任务下拉 + 按 paramDescs() 自动生成参数行。
// 用于 PreProcessNode / PostProcessNode。
Column {
    id: root

    property var node
    // 参数/任务变化时递增，用于强制重新求值（函数调用绑定不会自动刷新）
    property int revision: 0

    spacing: 6
    width: parent ? parent.width : implicitWidth

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    function taskLabel() {
        if (!node) return ""
        var opts = node.taskOptions()
        for (var i = 0; i < opts.length; ++i)
            if (opts[i].id === node.task) return opts[i].name
        return node.task
    }

    function paramValue(key) {
        if (!node) return ""
        var m = node.taskParams()
        return m[key] === undefined ? "" : m[key]
    }

    function boolValue(key) {
        return String(root.paramValue(key)) === "true"
    }

    function optionLabel(options, val) {
        if (!options) return "" + val
        for (var i = 0; i < options.length; ++i)
            if ("" + options[i].value === "" + val) return options[i].label
        return "" + val
    }

    Connections {
        target: root.node
        ignoreUnknownSignals: true
        function onParamsChanged() { root.revision++ }
        function onTaskPortsChanged() { root.revision++ }
    }

    // ---- 任务下拉 ----
    Rectangle {
        id: taskBox
        width: parent.width
        height: 24
        radius: 5
        color: taskHover.hovered || taskMenu.visible ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 20
            verticalAlignment: Text.AlignVCenter
            text: root.node ? (root.revision, root.taskLabel()) : ""
            color: Theme.fg
            font.pixelSize: 11
            elide: Text.ElideRight
            renderType: root.textRenderType
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.fgDim
            font.pixelSize: 11
            renderType: root.textRenderType
        }

        HoverHandler { id: taskHover }
        TapHandler { onTapped: taskMenu.visible ? taskMenu.close() : taskMenu.open() }

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

    // ---- 参数行：group>0 的参数排在同一行；特殊控件用自然宽度，其余等分 ----
    function naturalWidth(it) {
        if (it.kind === "size2") return 46 + 54 + 6 + 54          // 标签 + [W]×[H]
        if (it.kind === "floats") {
            var n = it.vecCount ? it.vecCount : 1
            return 46 + n * 52 + (n - 1) * 4
        }
        return -1
    }
    function labelW(it, count) {
        if (it.kind === "size2" || it.kind === "floats") return 46
        return count >= 3 ? 22 : count === 2 ? 24 : 46
    }
    readonly property var paramRows: {
        if (!node) return []
        root.revision
        var descs = node.paramDescs()
        var cur = node.taskParams()
        var groups = []
        for (var i = 0; i < descs.length; ++i) {
            var d = descs[i]
            if (d.showIfKey !== undefined && d.showIfKey !== "") {
                if (("" + cur[d.showIfKey]) !== ("" + d.showIfValue)) continue
            }
            var g = d.group === undefined ? 0 : d.group
            if (g > 0 && groups.length > 0 && groups[groups.length - 1].group === g)
                groups[groups.length - 1].items.push(d)
            else
                groups.push({ group: g, items: [d] })
        }
        // 计算每列的 labelWidth/controlWidth
        var rows = []
        for (var r = 0; r < groups.length; ++r) {
            var items = groups[r].items
            var specialSum = 0, flexCount = 0
            for (var j = 0; j < items.length; ++j) {
                if (naturalWidth(items[j]) >= 0) specialSum += naturalWidth(items[j])
                else ++flexCount
            }
            var gaps = Math.max(0, items.length - 1) * 8
            var flexArea = Math.max(0, root.width - specialSum - gaps)
            var perItem = flexCount > 0 ? flexArea / flexCount : 0
            var cols = []
            for (var k = 0; k < items.length; ++k) {
                var it = items[k]
                var lw = labelW(it, items.length)
                cols.push({ desc: it, labelWidth: lw,
                            controlWidth: naturalWidth(it) >= 0 ? 0
                                          : Math.min(110, Math.max(36, perItem - lw - 6)) })
            }
            rows.push({ group: groups[r].group, cols: cols })
        }
        return rows
    }

    Repeater {
        model: root.paramRows
        delegate: Row {
            id: groupRow
            required property var modelData
            width: root.width
            spacing: 8

            Repeater {
                model: groupRow.modelData.cols
                delegate: ParamField {
                    node: root.node
                    desc: groupRow.modelData.cols[index].desc
                    revision: root.revision
                    labelWidth: groupRow.modelData.cols[index].labelWidth
                    controlWidth: groupRow.modelData.cols[index].controlWidth
                }
            }
        }
    }
}
