# 任务节点紧凑布局实施计划（后处理 / 预处理）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把「后处理/预处理」节点的参数区与 Top-K 面板改成紧凑布局：任务下拉不再占满整行并与首个 bool 参数同行，其余参数排成两列网格；Top-K 改为细行列表。

**Architecture:** 纯 QML 视觉改造，不动 C++ 与执行逻辑。`ParamField` 从「标签列 + 控件」改为「标签内联进控件内部、宽度由父级给定」；`TaskNodeControls` 用一个 JS 计算属性把可见参数切分为「任务行参数 + 两列网格行」，再用 `Repeater` 渲染；`PostProcessNode` 的 Top-K 面板就地重写。

**Tech Stack:** Qt 6.11 Quick（QML）、QtQuick.Layouts 不需要（手工算列宽）、`qmllint` 静态检查。

**设计文档：** `docs/superpowers/specs/2026-09-22-ortdraw-task-node-compact-design.md`

**视觉稿：** `.superpowers/brainstorm/16333-1790079556/content/postprocess-a2b-topk.html`

**通用命令：** 构建 `cmake --build build -j4`；QML 检查 `/usr/lib/qt6/bin/qmllint qml/node/ParamField.qml qml/node/TaskNodeControls.qml qml/node/PostProcessNode.qml qml/node/PreProcessNode.qml`；实机 `DISPLAY=:0 ./bin/main`（程序在仓库根运行，QML 打进 qrc，**改 QML 后必须重新构建**）。

**本仓库约定（覆盖本计划的 commit 步骤）：** 只有在用户明确说「提交」时才执行 `git commit`。计划中每个 Task 末尾的 commit 步骤应先向用户汇报结果并等待确认；用户说提交后再执行。

---

## 文件结构

- Modify: `qml/node/ParamField.qml`（整文件重写：内联标签、外部定宽、20px 控件）
- Modify: `qml/node/TaskNodeControls.qml`（整文件重写：任务行 + 两列网格）
- Modify: `qml/node/PostProcessNode.qml`（仅 Top-K 面板块）
- 不改：`qml/node/PreProcessNode.qml`（复用 `TaskNodeControls`，自动获得新样式）、任何 C++ 文件

本计划不新增测试文件：QML 布局无法单测，验证手段为 `qmllint` + 实机手工核对（Task 3 给清单）。

---

## Task 1: ParamField 内联标签 + TaskNodeControls 两列网格（A2）

这两个文件是耦合的（`ParamField` 的新宽度语义会破坏旧调用点），必须在同一次改动中完成，避免中间态无法运行。

**Files:**
- Modify: `qml/node/ParamField.qml`（整文件替换）
- Modify: `qml/node/TaskNodeControls.qml`（整文件替换）

- [ ] **Step 1: 用下面的内容整体替换 `qml/node/ParamField.qml`**

```qml
import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 单个任务参数控件（紧凑版）：标签内联到控件内部左侧（灰字，10px），
// 高度统一 20；宽度由父级（任务行/网格列）给定。
Item {
    id: field

    property var node
    property var desc
    property int revision: 0

    implicitHeight: 20
    height: implicitHeight

    readonly property string kind: desc ? desc.kind : ""
    readonly property string key: (desc && desc.key !== undefined) ? desc.key : ""
    readonly property string labelText: desc ? desc.label : ""
    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    function val(k) {
        if (!node || k === "") return ""
        var m = node.taskParams()
        return m[k] === undefined ? "" : m[k]
    }
    function boolValue() { return String(field.val(field.key)) === "true" }
    function optionLabel(options, v) {
        if (!options) return "" + v
        for (var i = 0; i < options.length; ++i)
            if ("" + options[i].value === "" + v) return options[i].label
        return "" + v
    }
    function commit(v) {
        if (!node || field.key === "") return
        node.setTaskParam(field.key, v)
        NodeManager.commitNodeParams(node.uuid)
    }

    // ---- bool：标签 + 开关（无外框，开关靠右）----
    Row {
        id: boolRow
        visible: field.kind === "bool"
        width: field.width
        height: field.height
        spacing: 6

        Text {
            width: Math.max(0, boolRow.width - 36)
            height: boolRow.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        Item {
            width: 30
            height: boolRow.height
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 17
                radius: 8.5
                color: field.boolValue() ? Theme.blue : Theme.bg
                border.width: 1
                border.color: field.boolValue() ? Theme.blue : Theme.border
                Behavior on color { ColorAnimation { duration: 130 } }
                Rectangle {
                    width: 13
                    height: 13
                    radius: 6.5
                    y: 2
                    x: field.boolValue() ? 15 : 2
                    color: field.boolValue() ? "#ffffff" : Theme.fgDim
                    Behavior on x { NumberAnimation { duration: 130 } }
                }
                TapHandler { onTapped: field.commit(!field.boolValue()) }
            }
        }
    }

    // ---- select：标签 + 值 + ▾（一个内联框）----
    Rectangle {
        id: selectBox
        visible: field.kind === "select"
        width: field.width
        height: field.height
        radius: 5
        color: selectHover.hovered || selectMenu.visible ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            id: selectLabel
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, selectBox.width * 0.45))
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        Text {
            id: selectCaret
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Text {
            anchors.left: selectLabel.right
            anchors.leftMargin: 6
            anchors.right: selectCaret.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: (field.revision,
                   field.optionLabel(field.desc ? field.desc.options : null,
                                     field.val(field.key)))
            color: Theme.fg
            font.pixelSize: 11
            elide: Text.ElideRight
            renderType: field.textRenderType
        }

        HoverHandler { id: selectHover }
        TapHandler { onTapped: selectMenu.visible ? selectMenu.close() : selectMenu.open() }

        Menu {
            id: selectMenu
            parent: Overlay.overlay
            onAboutToShow: {
                var p = selectBox.mapToItem(null, 0, selectBox.height + 4)
                x = p.x
                y = p.y
            }
            background: Rectangle {
                implicitWidth: 150
                color: Theme.bgElev
                border.width: 1
                border.color: Theme.border
                radius: 8
            }
            Instantiator {
                model: field.desc ? field.desc.options : []
                delegate: MenuItem {
                    id: optItem
                    required property var modelData
                    implicitHeight: 26
                    text: modelData.label
                    onTriggered: field.commit(modelData.value)
                    contentItem: Text {
                        text: optItem.text
                        color: optItem.hovered ? Theme.fgBright : Theme.fg
                        font.pixelSize: 11
                        leftPadding: 12
                        rightPadding: 12
                        verticalAlignment: Text.AlignVCenter
                        renderType: field.textRenderType
                    }
                    background: Rectangle {
                        radius: 6
                        color: optItem.hovered ? Theme.bgHover : "transparent"
                    }
                }
                onObjectAdded: (index, object) => selectMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => selectMenu.removeItem(object)
            }
        }
    }

    // ---- floats：标签 + 每格 40px 输入框（逗号分隔存储）----
    Row {
        id: floatsRow
        visible: field.kind === "floats"
        width: field.width
        height: field.height
        spacing: 4

        readonly property var cells: ("" + field.val(field.key)).split(",")

        function joinCells() {
            var parts = []
            for (var i = 0; i < floatRepeater.count; ++i) {
                var it = floatRepeater.itemAt(i)
                if (it) parts.push(it.cellText)
            }
            field.commit(parts.join(","))
        }

        Text {
            height: floatsRow.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Repeater {
            id: floatRepeater
            model: field.desc ? field.desc.vecCount : 0
            delegate: Rectangle {
                id: cellBox
                required property int index
                property string cellText: floatsRow.cells[index] !== undefined
                                          ? ("" + floatsRow.cells[index]).trim() : ""
                width: 40
                height: field.height
                radius: 5
                color: Theme.bg
                border.width: 1
                border.color: cellIn.activeFocus ? Theme.blue : Theme.border

                TextInput {
                    id: cellIn
                    anchors.fill: parent
                    horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.fg
                    font.pixelSize: 11
                    selectByMouse: true
                    renderType: field.textRenderType
                    text: cellBox.cellText
                    onEditingFinished: floatsRow.joinCells()
                }

                Connections {
                    target: field
                    function onRevisionChanged() {
                        if (!cellIn.activeFocus)
                            cellIn.text = floatsRow.cells[cellBox.index] !== undefined
                                          ? ("" + floatsRow.cells[cellBox.index]).trim() : ""
                    }
                }
            }
        }
    }

    // ---- size2：标签 + W × H（每格 44px，存储为 "WxH"）----
    Row {
        id: size2Row
        visible: field.kind === "size2"
        width: field.width
        height: field.height
        spacing: 4

        // 引用 revision 以在参数变化（如下拉切换）时重新求值
        readonly property var parts: (field.revision,
            ("" + field.val(field.key)).split("x"))

        function commit2() {
            field.commit(parseInt(wCell.text, 10) + "x" + parseInt(hCell.text, 10))
        }

        Text {
            height: size2Row.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Rectangle {
            width: 44
            height: field.height
            radius: 5
            color: Theme.bg
            border.width: 1
            border.color: wCell.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: wCell
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg
                font.pixelSize: 11
                selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 0 ? ("" + size2Row.parts[0]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
        Text {
            height: size2Row.height
            verticalAlignment: Text.AlignVCenter
            text: "×"
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Rectangle {
            width: 44
            height: field.height
            radius: 5
            color: Theme.bg
            border.width: 1
            border.color: hCell.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: hCell
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg
                font.pixelSize: 11
                selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 1 ? ("" + size2Row.parts[1]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
    }

    // ---- int / float / text：内联标签 + 右对齐值 ----
    Rectangle {
        id: scalarBox
        visible: field.kind !== "bool" && field.kind !== "select"
                 && field.kind !== "floats" && field.kind !== "size2"
        width: field.width
        height: field.height
        radius: 5
        color: Theme.bg
        border.width: 1
        border.color: scalarIn.activeFocus ? Theme.blue : Theme.border

        Text {
            id: scalarLabel
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, scalarBox.width - 22))
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        TextInput {
            id: scalarIn
            anchors.left: scalarLabel.right
            anchors.leftMargin: 4
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: TextInput.AlignRight
            color: Theme.fg
            font.pixelSize: 11
            selectByMouse: true
            renderType: field.textRenderType
            text: (field.revision, "" + field.val(field.key))
            onEditingFinished: field.commit(text)
        }

        Connections {
            target: field
            function onRevisionChanged() {
                if (!scalarIn.activeFocus)
                    scalarIn.text = "" + field.val(field.key)
            }
        }
    }
}
```

- [ ] **Step 2: 用下面的内容整体替换 `qml/node/TaskNodeControls.qml`**

```qml
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

    TextMetrics { id: taskMetrics; font.pixelSize: 11 }

    // 粗略文本宽：中文字宽≈字号，ASCII≈0.56 倍（用于跨列判断，无需精确）
    function textWidth(s, px) {
        if (!s) return 0
        var w = 0
        for (var i = 0; i < s.length; ++i)
            w += s.charCodeAt(i) > 255 ? px : px * 0.56
        return w
    }
    // 参数的固有宽度；-1 表示“标量，恒为单列”
    function naturalWidth(d) {
        if (!d) return -1
        var lw = textWidth(d.label, 10) + 10
        if (d.kind === "floats") {
            var n = d.vecCount ? d.vecCount : 1
            return lw + n * 40 + (n - 1) * 4
        }
        if (d.kind === "size2") return lw + 44 + 14 + 44
        return -1
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

        // 网格：相邻两个非跨列参数成一行；跨列参数独占一行（不重排顺序）
        var rows = []
        var r = 0
        while (r < rest.length) {
            if (naturalWidth(rest[r]) > cellW) {
                rows.push({ cells: [{ desc: rest[r], w: W }] })
                ++r
            } else if (r + 1 < rest.length && naturalWidth(rest[r + 1]) <= cellW) {
                rows.push({ cells: [{ desc: rest[r], w: cellW },
                                    { desc: rest[r + 1], w: cellW }] })
                r += 2
            } else {
                rows.push({ cells: [{ desc: rest[r], w: cellW }] })
                ++r
            }
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
            width: Math.max(80, Math.min(132, taskMetrics.advanceWidth(root.taskLabel()) + 30))
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
                    node: root.node
                    desc: gridRow.modelData.cells[index].desc
                    revision: root.revision
                    width: gridRow.modelData.cells[index].w
                }
            }
        }
    }
}
```

- [ ] **Step 3: 静态检查（预期无 error / 无 warning）**

Run:
```bash
/usr/lib/qt6/bin/qmllint qml/node/ParamField.qml qml/node/TaskNodeControls.qml qml/node/PreProcessNode.qml
```
Expected: 无输出（或仅有既有的无害提示，且不包含 `Parameter is not declared` / `Cannot assign` 之类错误）。

- [ ] **Step 4: 构建（预期成功）**

Run: `cmake --build build -j4`
Expected: `Built target main`（QML 进 qrc，必须重新构建才会生效）。

- [ ] **Step 5: 实机核对（后处理 5 个任务中的 3 个）**

Run: `DISPLAY=:0 ./bin/main`，然后：
1. 添加「后处理」节点，任务保持 `YOLO 检测`：第一行应为 `[YOLO 检测 ▾] … 画分数 ⏻`；下面两行是 `置信|IoU`、`框数|线宽`；节点明显比改版前矮。
2. 切换到 `YOLO 分割`：任务行右侧应为「置信」输入框（无 bool），其余参数在网格中。
3. 切换到 `分类`：任务行右侧应为「Top-K」输入框；把 Top-K 改成 3，确认参数行仍是一行（Top-K 面板此时还是旧样式，Task 2 处理）。
4. 截图留证：`import -window "$(xwininfo -root -tree | grep -i ortdraw | grep -o '^ *0x[0-9a-f]*' | head -1 | tr -d ' ')" /tmp/opencode/task1-yolo.png`

Expected: 布局与视觉稿 A2 一致；控件无重叠、无文字截断到不可读；点击任务下拉能正常切换并即时刷新参数区。

> **Task 1 评审后修订（控制器追加，优先于上面 Step 2 的代码）：**
> 1. `naturalWidth()` 增加 `select` 分支：`textWidth(d.label, 10) + 10 + 最长选项文本宽(按 11px 估) + 26`；超过 `cellW` 时该 select 独占整行。理由：避免「处理方式 = 逐通道 Min-Max」在 131px 单列里被省略号截断（spec §3.2 已同步更新）。用**最长选项**而非当前值，保证切换选项时布局稳定。`int/float/text` 仍恒为单列。
> 2. 内层网格委托直接使用 `modelData.desc` / `modelData.w`（不再用 `index` 回查 `cells` 数组）。
> 3. bool 标签宽度改为按开关实际宽度计算（`boolRow.width - toggleItem.width - boolRow.spacing`），去掉硬编码的 36。

- [ ] **Step 6: 汇报并等待用户确认后提交**

向用户汇报：改了哪两个文件、`A2` 布局效果、截图路径。用户说「提交」后再执行：
```bash
git add qml/node/ParamField.qml qml/node/TaskNodeControls.qml
git commit -m "feat(ui): compact task-node param layout (task row + two-column grid)"
```

---

## Task 2: PostProcessNode 的 Top-K 面板改为细行列表（T1）

**Files:**
- Modify: `qml/node/PostProcessNode.qml`（只替换 Top-K 面板块）

- [ ] **Step 1: 用下面的代码替换 `PostProcessNode.qml` 中原来的 Top-K 面板块**

原块以注释 `// ---- 分类 Top-K 结果面板（非端口显示数据）----` 开头，到该 `Column` 结束为止（即 `NodeCard { Column { TaskNodeControls … , Column { …topk… } } }` 中的第二个 `Column`）。替换为：

```qml
            // ---- 分类 Top-K 结果面板（非端口显示数据）----
            Item {
                visible: root.hasTopk
                width: parent.width
                height: root.hasTopk ? (12 + root.topkList.length * 12) : 0

                Item {
                    width: parent.width
                    height: 12

                    Text {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        text: "类别"
                        color: Theme.fgDim
                        font.pixelSize: 9
                        renderType: root.textRenderType
                    }
                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "置信度"
                        color: Theme.fgDim
                        font.pixelSize: 9
                        renderType: root.textRenderType
                    }
                }

                Column {
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    width: parent.width
                    spacing: 0

                    Repeater {
                        model: root.topkList
                        delegate: Row {
                            id: barRow
                            required property var modelData
                            width: parent.width
                            height: 12
                            spacing: 6

                            Text {
                                height: barRow.height
                                width: 34
                                verticalAlignment: Text.AlignVCenter
                                text: "类 " + barRow.modelData.id
                                color: Theme.fg
                                font.pixelSize: 10
                                elide: Text.ElideRight
                                renderType: root.textRenderType
                            }
                            Item {
                                width: Math.max(0, barRow.width - 34 - 34 - barRow.spacing * 2)
                                height: barRow.height

                                Rectangle {
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: parent.width
                                    height: 6
                                    radius: 3
                                    color: Theme.bg
                                    clip: true

                                    Rectangle {
                                        width: parent.width * Math.max(0, Math.min(1,
                                                   barRow.modelData.score))
                                        height: parent.height
                                        radius: 3
                                        color: Theme.blue
                                    }
                                }
                            }
                            Text {
                                height: barRow.height
                                width: 34
                                verticalAlignment: Text.AlignVCenter
                                horizontalAlignment: Text.AlignRight
                                text: "" + (Math.round(barRow.modelData.score * 1000) / 1000)
                                color: Theme.fgDim
                                font.pixelSize: 10
                                renderType: root.textRenderType
                            }
                        }
                    }
                }
            }
```

- [ ] **Step 2: 静态检查**

Run: `/usr/lib/qt6/bin/qmllint qml/node/PostProcessNode.qml`
Expected: 无错误。

- [ ] **Step 3: 构建**

Run: `cmake --build build -j4`
Expected: `Built target main`。

- [ ] **Step 4: 实机核对（分类任务的 Top-K）**

Run: `DISPLAY=:0 ./bin/main`，添加「后处理」节点，任务切到 `分类`：
1. 未运行前不显示面板（`displayData` 为空）。
2. 连好 `图像 → 分类 → ?`（或在已有分类链路上重新运行），面板应显示：一行 9px 小标题「类别 / 置信度」+ 每行 12px 的 `类 N ▏▏▏▏ 0.xxx`。
3. 把 `Top-K` 从 5 改成 3 再运行：行数变 3；改成 8：行数变 8，第 8 行贴到 1.0 或按分数比例显示。
4. 截图：`import -window "$(xwininfo -root -tree | grep -i ortdraw | grep -o '^ *0x[0-9a-f]*' | head -1 | tr -d ' ')" /tmp/opencode/task2-topk.png`

Expected: 无冗余标题行；条形为 6px 细条；分数右对齐；行高紧凑。

- [ ] **Step 5: 汇报并等待用户确认后提交**

用户说「提交」后执行：
```bash
git add qml/node/PostProcessNode.qml
git commit -m "feat(ui): compact Top-K list in post-process node"
```

---

## Task 3: 全量回归与验收

**Files:** 无改动（只验证；发现问题回到 Task 1/2 修）。

- [ ] **Step 1: 后处理三个任务的布局与联动**

逐一切换 `YOLO 检测 / YOLO 分割 / 分类`，确认：
- 任务下拉宽度随任务名变化，但不超 132px、不小于 80px（可用「YOLO 检测」「YOLO 分割」「分类」三个名字对比）。
- 切换任务后端口（输入/输出）与参数区**同时**刷新，且不残留上一个任务的参数行。
- `YOLO 检测`：`画分数` 开关在任务行右侧，点击立即变色并在下次运行生效；`线宽` 提交后生效。
- 修改任一参数回车后，节点高度/内容不闪烁错位。

- [ ] **Step 2: 预处理任务的布局与联动（关键回归点）**

添加「预处理」节点，任务分别切到 `标准预处理` / `YOLO 预处理`：
- `标准预处理`：任务行右侧应为「布局」下拉；网格中 `通道|精度`、`处理方式`；把 `处理方式` 改为 `Z-score` → 出现 `均值`/`标准差`（各独占整行，因为跨列）；改为 `逐通道Min-Max` → 出现对应 min/max 行；`尺寸` 改为 `指定` → 出现 `宽x高`（整行）。
- `YOLO 预处理`：`尺寸`/`填充值`/`精度` 正常显示与提交。
- 参数跨列时**不重排顺序**（例如「处理方式」单独占左列、右列留空是预期）。

- [ ] **Step 3: 主题 / 缩放 / 高度自适应**

1. `Ctrl+=` / `Ctrl+-` 切到 0.8x 与 1.25x：文本仍清晰，控件无重叠；任务下拉文本不溢出。
2. 设置里切明/暗主题：所有新控件颜色跟随主题（无写死深色）。
3. 关闭 `Settings.autoHeight`：节点高度固定为 `minHeight`，参数区可能被裁切但不崩溃；重新打开后高度恢复。
4. 把节点宽度拖到 `min_width=280` 与 520：280 时两列仍不重叠；520 时控件不无限拉伸。

- [ ] **Step 4: 队列/执行回归（确认没有连带破坏）**

1. 运行一条 `图像 → 预处理 → ONNX 推理 → 后处理` 链路（用 `models/yolo11n.onnx`），确认队列条目、耗时、输出图像正常。
2. 失败一次（例如断开 ONNX 输入）确认错误提示/失败芯片仍正常。

- [ ] **Step 5: 汇报验收结果**

向用户汇报：Task 1/2 的截图、Task 3 各项结果、发现的任何问题与修复。等待用户决定是否提交剩余改动（若有）。

---

## 备注

- `qmllint` 若对 `TextMetrics.advanceWidth` 报未知方法，请确认 `qt6-declarative >= 6.4`（本机 6.11.2）；仍报错时改用隐藏 `Text { text: …; font.pixelSize: 11 }` 的 `implicitWidth` 作为任务下拉宽度来源。
- 若 `Row` 内出现 `Cannot anchor to an item that isn't a parent` 警告，检查是否在 `Row/Column` 子项上用了 `anchors`——本计划已把所有 Row 子项改为等高的 `Item/Rectangle` 包装，不应触发。
- 视觉稿中的细线（任务行与网格之间的分隔）**不实现**。
