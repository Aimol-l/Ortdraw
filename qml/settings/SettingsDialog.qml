import QtQuick
import QtQuick.Controls
import Theme
import Settings
import UiBus
import "controls"

// 全窗口设置浮层：按 docs/mockups/ortdraw-settings-v1.html 移植
Item {
    id: root
    anchors.fill: parent
    visible: false
    z: 200
    onVisibleChanged: UiBus.overlayOpen = visible

    property string query: ""
    property string activeSection: "appearance"
    property var snapshot: null
    property var accentChoices: ["#2e7de9", "#9854f1", "#007197", "#587539", "#b15c00"]

    // ---- 生命周期 ----
    function open() {
        snapshot = capture()
        activeSection = "appearance"
        searchField.text = ""
        query = ""
        visible = true
        applyFilter()
    }

    function close() { visible = false }

    function cancel() {
        restore(snapshot)
        close()
    }

    function capture() {
        return {
            theme: Settings.theme,
            accentColor: ("" + Settings.accentColor).toLowerCase(),
            accentCustom: Settings.accentCustom,
            density: Settings.density,
            language: Settings.language,
            backgroundMode: Settings.backgroundMode,
            snapToGrid: Settings.snapToGrid,
            gridSpacing: Settings.gridSpacing,
            spaceToPan: Settings.spaceToPan,
            fitMargin: Settings.fitMargin,
            zoomMin: Settings.zoomMin,
            zoomMax: Settings.zoomMax,
            showPreview: Settings.showPreview,
            previewHeight: Settings.previewHeight,
            showPortTypeTags: Settings.showPortTypeTags,
            autoHeight: Settings.autoHeight,
            textRender: Settings.textRender,
            cornerRadius: Settings.cornerRadius,
            renderMode: Settings.renderMode,
            linkWidth: Settings.linkWidth,
            midpointMode: Settings.midpointMode,
            hoverHighlight: Settings.hoverHighlight,
            ctrlMultiSelect: Settings.ctrlMultiSelect,
            contextMenu: Settings.contextMenu,
            confirmDelete: Settings.confirmDelete,
            connectMode: Settings.connectMode,
            autoDisconnect: Settings.autoDisconnect,
            minimapFps: Settings.minimapFps,
            antialias: Settings.antialias,
            asyncImage: Settings.asyncImage
        }
    }

    function restore(s) {
        if (!s) return
        Settings.theme = s.theme
        if (s.accentCustom)
            Settings.accentColor = s.accentColor
        else
            Settings.clearAccentCustom()
        Settings.density = s.density
        Settings.language = s.language
        Settings.backgroundMode = s.backgroundMode
        Settings.snapToGrid = s.snapToGrid
        Settings.gridSpacing = s.gridSpacing
        Settings.spaceToPan = s.spaceToPan
        Settings.fitMargin = s.fitMargin
        Settings.zoomMin = s.zoomMin
        Settings.zoomMax = s.zoomMax
        Settings.showPreview = s.showPreview
        Settings.previewHeight = s.previewHeight
        Settings.showPortTypeTags = s.showPortTypeTags
        Settings.autoHeight = s.autoHeight
        Settings.textRender = s.textRender
        Settings.cornerRadius = s.cornerRadius
        Settings.renderMode = s.renderMode
        Settings.linkWidth = s.linkWidth
        Settings.midpointMode = s.midpointMode
        Settings.hoverHighlight = s.hoverHighlight
        Settings.ctrlMultiSelect = s.ctrlMultiSelect
        Settings.contextMenu = s.contextMenu
        Settings.confirmDelete = s.confirmDelete
        Settings.connectMode = s.connectMode
        Settings.autoDisconnect = s.autoDisconnect
        Settings.minimapFps = s.minimapFps
        Settings.antialias = s.antialias
        Settings.asyncImage = s.asyncImage
    }

    // ---- 搜索 / 分类过滤 ----
    function allSections() {
        return [secAppearance, secCanvas, secNodes, secLinks, secInteraction, secPerf, secKeys, secAbout]
    }

    function applyFilter() {
        var q = ("" + query).trim().toLowerCase()
        var secs = allSections()
        var any = false
        for (var i = 0; i < secs.length; i++) {
            var sec = secs[i]
            var secHit = false
            var kids = sec.children
            for (var j = 0; j < kids.length; j++) {
                var c = kids[j]
                if (c && c.isRow === true) {
                    var hit = (q === "") ? true : c.matchesQuery(q)
                    c.visible = hit
                    if (hit) secHit = true
                } else if (c && c.isGroup === true) {
                    c.visible = (q === "")
                }
            }
            sec.visible = (q === "") ? (sec.category === activeSection) : secHit
            if (secHit) any = true
        }
        emptyState.visible = (q !== "" && !any)
    }

    // ---- 缩放范围预设 ----
    function currentZoomPreset() {
        if (Settings.zoomMin <= 0.11 && Settings.zoomMax >= 3.9) return "wide"
        return "default"
    }
    function applyZoomPreset(v) {
        if (v === "wide") {
            Settings.zoomMin = 0.1
            Settings.zoomMax = 4.0
        } else {
            Settings.zoomMin = 0.35
            Settings.zoomMax = 2.4
        }
    }

    // ===== 遮罩 =====
    Rectangle {
        anchors.fill: parent
        color: Theme.dark ? Qt.rgba(0, 0, 0, 0.5) : Qt.rgba(0.118, 0.137, 0.235, 0.35)
        MouseArea {
            anchors.fill: parent
            onClicked: root.cancel()
        }
    }

    // ===== 对话框卡片 =====
    Rectangle {
        id: card
        anchors.centerIn: parent
        width: Math.min(900, parent.width - 40)
        height: Math.min(620, parent.height - 40)
        radius: 12
        color: Theme.bgPanel
        border.width: 1
        border.color: Theme.border

        MouseArea { anchors.fill: parent; onClicked: {} }

        Column {
            anchors.fill: parent

            // ---- 头部 ----
            Rectangle {
                id: header
                width: parent.width
                height: 56
                color: "transparent"

                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.borderSoft
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 12

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "设置"
                        color: Theme.fgBright
                        font.pixelSize: 15
                        font.bold: true
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Ortdraw 偏好设置"
                        color: Theme.fgDim
                        font.pixelSize: 12
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 12
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    TextField {
                        id: searchField
                        anchors.verticalCenter: parent.verticalCenter
                        width: 240
                        height: 32
                        placeholderText: "搜索设置项…"
                        placeholderTextColor: Theme.fgDim
                        color: Theme.fg
                        font.pixelSize: 13
                        leftPadding: 30
                        rightPadding: 10
                        selectByMouse: true
                        onTextChanged: {
                            root.query = text
                            root.applyFilter()
                        }
                        background: Rectangle {
                            color: Theme.bg
                            border.width: 1
                            border.color: searchField.activeFocus ? Theme.blue : Theme.border
                            radius: 8
                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                text: "⌕"
                                color: Theme.fgDim
                                font.pixelSize: 14
                            }
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28
                        height: 28
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: Theme.dark ? "☀" : "☾"
                        color: themeArea.containsMouse ? Theme.fgBright : Theme.fgDim
                        font.pixelSize: 15
                        MouseArea {
                            id: themeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Theme.toggle()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 28
                        height: 28
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        text: "×"
                        color: closeArea.containsMouse ? Theme.red : Theme.fgDim
                        font.pixelSize: 18
                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.cancel()
                        }
                    }
                }
            }

            // ---- 主体 ----
            Row {
                id: bodyRow
                width: parent.width
                height: parent.height - header.height - footer.height
                spacing: 0

                // 左侧分类导航
                Item {
                    id: navWrap
                    width: 190
                    height: parent.height

                    Column {
                        id: nav
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 10
                        spacing: 3

                        Repeater {
                            model: [
                                { key: "appearance", icon: "◐", label: "外观" },
                                { key: "canvas", icon: "▦", label: "画布" },
                                { key: "nodes", icon: "◇", label: "节点" },
                                { key: "links", icon: "⌇", label: "连线" },
                                { key: "interaction", icon: "☞", label: "交互" },
                                { key: "perf", icon: "⚡", label: "性能" },
                                { key: "keys", icon: "⌨", label: "快捷键" },
                                { key: "about", icon: "ⓘ", label: "关于" }
                            ]

                            delegate: Rectangle {
                                required property var modelData

                                width: nav.width
                                height: 34
                                radius: 8
                                color: root.activeSection === modelData.key
                                       ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.12)
                                       : (navArea.containsMouse ? Theme.bgHover : "transparent")

                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 11
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 10

                                    Text {
                                        width: 16
                                        horizontalAlignment: Text.AlignHCenter
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.icon
                                        color: root.activeSection === modelData.key ? Theme.blue : Theme.fgDim
                                        font.pixelSize: 13
                                    }
                                    Text {
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.label
                                        color: root.activeSection === modelData.key ? Theme.blue : Theme.fg
                                        font.pixelSize: 13
                                        font.bold: root.activeSection === modelData.key
                                    }
                                }

                                MouseArea {
                                    id: navArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.activeSection = modelData.key
                                        searchField.text = ""
                                        root.applyFilter()
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        anchors.right: parent.right
                        width: 1
                        height: parent.height
                        color: Theme.borderSoft
                    }
                }

                // 右侧内容
                Item {
                    id: contentWrap
                    width: bodyRow.width - navWrap.width
                    height: parent.height

                    Flickable {
                        id: flick
                        anchors.fill: parent
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        clip: true
                        contentWidth: width
                        contentHeight: contentCol.height + 40
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                        Column {
                            id: contentCol
                            y: 8
                            width: flick.width - 14

                            // ===== 外观 =====
                            Column {
                                id: secAppearance
                                property string category: "appearance"
                                width: parent.width

                                GroupTitle { text: "主题" }

                                SettingRow {
                                    title: "配色主题"
                                    desc: "亮色为默认；暗色为 Tokyo Night"
                                    keywords: "theme 主题 亮 暗 light dark"
                                    SegControl {
                                        options: [{ value: "light", label: "亮色" }, { value: "dark", label: "暗色" }]
                                        value: Settings.theme
                                        onPicked: (v) => Settings.theme = v
                                    }
                                }

                                SettingRow {
                                    title: "强调色"
                                    desc: "选中、连线、按钮等的强调色"
                                    keywords: "accent 强调色 主题色 color"
                                    SwatchControl {
                                        choices: root.accentChoices
                                        current: ("" + Settings.accentColor).toLowerCase()
                                        onPicked: (c) => Settings.accentColor = c
                                    }
                                }

                                GroupTitle { text: "界面" }

                                SettingRow {
                                    title: "界面密度"
                                    desc: "调整间距与控件大小"
                                    keywords: "density 密度 紧凑 标准 宽松"
                                    placeholder: true
                                    SegControl {
                                        options: [{ value: "compact", label: "紧凑" }, { value: "standard", label: "标准" }, { value: "comfortable", label: "宽松" }]
                                        value: Settings.density
                                        onPicked: (v) => Settings.density = v
                                    }
                                }

                                SettingRow {
                                    title: "语言"
                                    desc: "界面显示语言"
                                    keywords: "language 语言 中文 english"
                                    placeholder: true
                                    SelectControl {
                                        options: [{ value: "zh_CN", label: "简体中文" }, { value: "en_US", label: "English" }]
                                        value: Settings.language
                                        onPicked: (v) => Settings.language = v
                                    }
                                }
                            }

                            // ===== 画布 =====
                            Column {
                                id: secCanvas
                                property string category: "canvas"
                                width: parent.width

                                GroupTitle { text: "网格" }

                                SettingRow {
                                    title: "画布背景"
                                    desc: "空白 / 点阵 / 网格"
                                    keywords: "background grid 背景 网格 点阵 空白"
                                    SegControl {
                                        options: [
                                            { value: "none", label: "空白" },
                                            { value: "dots", label: "点阵" },
                                            { value: "grid", label: "网格" }
                                        ]
                                        value: Settings.backgroundMode
                                        onPicked: (v) => Settings.backgroundMode = v
                                    }
                                }

                                SettingRow {
                                    title: "网格吸附"
                                    desc: "拖动节点时对齐到网格"
                                    keywords: "snap 吸附 对齐 网格"
                                    SwitchControl {
                                        checked: Settings.snapToGrid
                                        onToggled: (v) => Settings.snapToGrid = v
                                    }
                                }

                                SettingRow {
                                    title: "网格间距"
                                    desc: "点阵间距（像素）"
                                    keywords: "spacing 间距 网格 像素"
                                    SliderControl {
                                        from: 10; to: 60
                                        value: Settings.gridSpacing
                                        onMoved: (v) => Settings.gridSpacing = v
                                    }
                                }

                                GroupTitle { text: "视图" }

                                SettingRow {
                                    title: "按住空格拖动平移画布"
                                    desc: "关闭后可直接拖空白平移"
                                    keywords: "space 空格 平移 pan"
                                    SwitchControl {
                                        checked: Settings.spaceToPan
                                        onToggled: (v) => Settings.spaceToPan = v
                                    }
                                }

                                SettingRow {
                                    title: "适应视图边距"
                                    desc: "适应视图时四周保留的空白"
                                    keywords: "fit 适应 边距 margin"
                                    SliderControl {
                                        from: 20; to: 200
                                        value: Settings.fitMargin
                                        onMoved: (v) => Settings.fitMargin = v
                                    }
                                }

                                SettingRow {
                                    title: "缩放范围"
                                    desc: "最小 / 最大缩放比例"
                                    keywords: "zoom 缩放 范围"
                                    SelectControl {
                                        options: [
                                            { value: "default", label: "35% – 240%" },
                                            { value: "wide", label: "10% – 400%" }
                                        ]
                                        value: root.currentZoomPreset()
                                        onPicked: (v) => root.applyZoomPreset(v)
                                    }
                                }
                            }

                            // ===== 节点 =====
                            Column {
                                id: secNodes
                                property string category: "nodes"
                                width: parent.width

                                GroupTitle { text: "显示" }

                                SettingRow {
                                    title: "显示预览缩略图"
                                    desc: "在节点内显示输出预览（可点击放大）"
                                    keywords: "preview 预览 缩略图"
                                    SwitchControl {
                                        checked: Settings.showPreview
                                        onToggled: (v) => Settings.showPreview = v
                                    }
                                }

                                SettingRow {
                                    title: "缩略图高度"
                                    desc: "节点内预览缩略图的高度"
                                    keywords: "preview 缩略图 高度 height"
                                    SliderControl {
                                        from: 60; to: 160
                                        value: Settings.previewHeight
                                        onMoved: (v) => Settings.previewHeight = v
                                    }
                                }

                                SettingRow {
                                    title: "显示端口类型标签"
                                    desc: "在端口名旁显示 Image / Int 等类型"
                                    keywords: "端口 类型 标签 port tag"
                                    SwitchControl {
                                        checked: Settings.showPortTypeTags
                                        onToggled: (v) => Settings.showPortTypeTags = v
                                    }
                                }

                                SettingRow {
                                    title: "节点高度自适应内容"
                                    desc: "根据端口与参数自动调整高度"
                                    keywords: "高度 自适应 auto height"
                                    SwitchControl {
                                        checked: Settings.autoHeight
                                        onToggled: (v) => Settings.autoHeight = v
                                    }
                                }

                                GroupTitle { text: "文字与外观" }

                                SettingRow {
                                    title: "文字渲染方式"
                                    desc: "曲线渲染缩放不糊；原生渲染更锐利"
                                    keywords: "文字 渲染 curve native 曲线 原生"
                                    SegControl {
                                        options: [{ value: "curve", label: "曲线" }, { value: "native", label: "原生" }]
                                        value: Settings.textRender
                                        onPicked: (v) => Settings.textRender = v
                                    }
                                }

                                SettingRow {
                                    title: "节点圆角"
                                    desc: "节点卡片圆角半径"
                                    keywords: "圆角 radius corner"
                                    SliderControl {
                                        from: 0; to: 20
                                        value: Settings.cornerRadius
                                        onMoved: (v) => Settings.cornerRadius = v
                                    }
                                }
                            }

                            // ===== 连线 =====
                            Column {
                                id: secLinks
                                property string category: "links"
                                width: parent.width

                                GroupTitle { text: "渲染" }

                                SettingRow {
                                    title: "连线渲染模式"
                                    desc: "与 ComfyUI 一致：Spline 平滑 / Linear 圆角折线 / Straight 直线"
                                    keywords: "连线 渲染 spline linear straight 曲线 折线 直线"
                                    LinkModeControl {
                                        value: Settings.renderMode
                                        onPicked: (v) => Settings.renderMode = v
                                    }
                                }

                                SettingRow {
                                    title: "连线线宽"
                                    desc: "连线描边宽度"
                                    keywords: "线宽 width 连线"
                                    SliderControl {
                                        from: 1; to: 5
                                        value: Settings.linkWidth
                                        onMoved: (v) => Settings.linkWidth = v
                                    }
                                }

                                GroupTitle { text: "选中与提示" }

                                SettingRow {
                                    title: "连线中点显示"
                                    desc: "控制中点圆点何时显示"
                                    keywords: "中点 显示 midpoint"
                                    SegControl {
                                        options: [
                                            { value: "selected", label: "选中" },
                                            { value: "hover", label: "悬停" },
                                            { value: "always", label: "始终" },
                                            { value: "never", label: "从不" }
                                        ]
                                        value: Settings.midpointMode
                                        onPicked: (v) => Settings.midpointMode = v
                                    }
                                }

                                SettingRow {
                                    title: "悬停高亮连线"
                                    desc: "鼠标悬停时高亮所在连线"
                                    keywords: "悬停 高亮 hover highlight"
                                    SwitchControl {
                                        checked: Settings.hoverHighlight
                                        onToggled: (v) => Settings.hoverHighlight = v
                                    }
                                }
                            }

                            // ===== 交互 =====
                            Column {
                                id: secInteraction
                                property string category: "interaction"
                                width: parent.width

                                GroupTitle { text: "选择与编辑" }

                                SettingRow {
                                    title: "Ctrl + 点击多选节点"
                                    desc: "按住 Ctrl 点击可多选节点"
                                    keywords: "ctrl 多选 选择 multi"
                                    SwitchControl {
                                        checked: Settings.ctrlMultiSelect
                                        onToggled: (v) => Settings.ctrlMultiSelect = v
                                    }
                                }

                                SettingRow {
                                    title: "删除前确认"
                                    desc: "删除节点时弹出确认"
                                    keywords: "删除 确认 delete confirm"
                                    placeholder: true
                                    SwitchControl {
                                        checked: Settings.confirmDelete
                                        onToggled: (v) => Settings.confirmDelete = v
                                    }
                                }

                                SettingRow {
                                    title: "启用右键菜单"
                                    desc: "在节点 / 连线 / 画布上右键打开菜单"
                                    keywords: "右键 菜单 context menu"
                                    SwitchControl {
                                        checked: Settings.contextMenu
                                        onToggled: (v) => Settings.contextMenu = v
                                    }
                                }

                                GroupTitle { text: "连接" }

                                SettingRow {
                                    title: "连线方式"
                                    desc: "点击端口依次连接，或拖拽连接"
                                    keywords: "连线 方式 点击 拖拽 connect"
                                    SegControl {
                                        options: [{ value: "drag", label: "拖拽" }, { value: "click", label: "点击" }]
                                        value: Settings.connectMode
                                        onPicked: (v) => Settings.connectMode = v
                                    }
                                }

                                SettingRow {
                                    title: "连接时自动断开旧连线"
                                    desc: "输入端口已有连线时自动替换"
                                    keywords: "自动 断开 连接 disconnect"
                                    placeholder: true
                                    SwitchControl {
                                        checked: Settings.autoDisconnect
                                        onToggled: (v) => Settings.autoDisconnect = v
                                    }
                                }
                            }

                            // ===== 性能 =====
                            Column {
                                id: secPerf
                                property string category: "perf"
                                width: parent.width

                                GroupTitle { text: "渲染" }

                                SettingRow {
                                    title: "Minimap 刷新频率上限"
                                    desc: "拖动时限制重绘频率"
                                    keywords: "minimap 刷新 fps 频率"
                                    SelectControl {
                                        options: [
                                            { value: 30, label: "30 FPS" },
                                            { value: 60, label: "60 FPS" },
                                            { value: 0, label: "不限制" }
                                        ]
                                        value: Settings.minimapFps
                                        onPicked: (v) => Settings.minimapFps = v
                                    }
                                }

                                SettingRow {
                                    title: "异步加载图像"
                                    desc: "后台线程加载图片，避免卡顿"
                                    keywords: "异步 加载 图像 async image"
                                    placeholder: true
                                    SwitchControl {
                                        checked: Settings.asyncImage
                                        onToggled: (v) => Settings.asyncImage = v
                                    }
                                }

                                SettingRow {
                                    title: "画布抗锯齿"
                                    desc: "平滑渲染连线与节点边缘"
                                    keywords: "抗锯齿 antialias 平滑"
                                    SwitchControl {
                                        checked: Settings.antialias
                                        onToggled: (v) => Settings.antialias = v
                                    }
                                }
                            }

                            // ===== 快捷键 =====
                            Column {
                                id: secKeys
                                property string category: "keys"
                                width: parent.width

                                GroupTitle { text: "编辑器快捷键" }

                                Row {
                                    width: parent.width
                                    spacing: 26

                                    Column {
                                        width: (parent.width - 26) / 2
                                        KeyRow { label: "删除选中节点 / 连线"; keys: "Delete" }
                                        KeyRow { label: "撤销"; keys: "Ctrl + Z" }
                                        KeyRow { label: "重做"; keys: "Ctrl + Y" }
                                        KeyRow { label: "复制选中节点"; keys: "Ctrl + D" }
                                        KeyRow { label: "运行 / 停止"; keys: "F5" }
                                    }
                                    Column {
                                        width: (parent.width - 26) / 2
                                        KeyRow { label: "适应视图"; keys: "Ctrl + 0" }
                                        KeyRow { label: "平移画布"; keys: "按住空格" }
                                        KeyRow { label: "多选节点"; keys: "Ctrl + 点击" }
                                        KeyRow { label: "关闭菜单 / 取消连线"; keys: "Esc" }
                                    }
                                }

                                GroupTitle { text: "文件" }

                                Row {
                                    width: parent.width
                                    spacing: 26

                                    Column {
                                        width: (parent.width - 26) / 2
                                        KeyRow { label: "保存图"; keys: "Ctrl + S" }
                                    }
                                    Column {
                                        width: (parent.width - 26) / 2
                                        KeyRow { label: "打开图"; keys: "Ctrl + O" }
                                        KeyRow { label: "新建图"; keys: "Ctrl + N" }
                                    }
                                }
                            }

                            // ===== 关于 =====
                            Column {
                                id: secAbout
                                property string category: "about"
                                width: parent.width

                                GroupTitle { text: "关于 Ortdraw" }

                                SettingRow {
                                    title: "版本"
                                    desc: "基于节点的深度学习图像处理工具（原型）"
                                    keywords: "版本 version about 关于"
                                    Badge { text: "v0.2.0" }
                                }

                                SettingRow {
                                    title: "构建依赖"
                                    desc: "Qt 6 · OpenCV · C++23"
                                    keywords: "构建 依赖 qt opencv c++"
                                    Row {
                                        spacing: 7
                                        Badge { text: "Qt 6.11" }
                                        Badge { text: "OpenCV 5" }
                                    }
                                }

                                SettingRow {
                                    title: "规划"
                                    desc: "ONNX Runtime 推理节点"
                                    keywords: "规划 roadmap onnx"
                                    Badge { text: "未接入" }
                                }

                                SettingRow {
                                    title: "开源许可"
                                    desc: "第三方组件许可"
                                    keywords: "许可 license 开源"
                                    placeholder: true
                                    GhostButton { label: "查看许可" }
                                }
                            }
                        }
                    }

                    Text {
                        id: emptyState
                        anchors.centerIn: contentWrap
                        visible: false
                        text: "没有匹配的设置项"
                        color: Theme.fgDim
                        font.pixelSize: 13
                    }
                }
            }

            // ---- 底部 ----
            Rectangle {
                id: footer
                width: parent.width
                height: 60
                color: "transparent"

                Rectangle {
                    anchors.top: parent.top
                    width: parent.width
                    height: 1
                    color: Theme.borderSoft
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    text: "更改会立即生效"
                    color: Theme.fgDim
                    font.pixelSize: 12
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 10

                    GhostButton {
                        label: "恢复默认"
                        onClicked: Settings.resetDefaults()
                    }
                    GhostButton {
                        label: "取消"
                        onClicked: root.cancel()
                    }
                    PrimaryButton {
                        label: "保存"
                        onClicked: {
                            Settings.sync()
                            root.close()
                        }
                    }
                }
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.visible
        onActivated: root.cancel()
    }

    Connections {
        target: UiBus
        function onSettingsRequested() { root.open() }
    }

    Component.onCompleted: applyFilter()

    // 消费滚轮，避免穿透到画布缩放（弹层内的 ScrollView 仍可正常滚动）
    WheelHandler { }
}
