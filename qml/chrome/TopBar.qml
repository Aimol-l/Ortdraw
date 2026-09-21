import QtQuick
import QtQuick.Controls as Controls
import Theme
import UiBus
import Settings
import NodeManager
import Log

Rectangle {
    id: root

    height: 46
    color: Theme.bgPanel

    signal undoRequested()
    signal redoRequested()
    signal fitRequested()
    signal clearRequested()
    signal newRequested()
    signal openRequested()
    signal saveRequested()
    signal saveAsRequested()

    property string docName: ""
    property bool docDirty: false
    property string docPath: ""

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        id: leftCluster
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 9

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 24
                height: 24
                radius: 7
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.blue }
                    GradientStop { position: 1.0; color: Theme.magenta }
                }

                Text {
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    anchors.centerIn: parent
                    text: "O"
                    color: "#ffffff"
                    font.pixelSize: 13
                    font.bold: true
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0

                Text {
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    text: "Ortdraw"
                    color: Theme.fgBright
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    text: "节点式图像处理"
                    color: Theme.fgDim
                    font.pixelSize: 10
                }
            }
        }

        // 当前文档名（+ 未保存标记），悬停显示完整路径
        Row {
            id: docLabel
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
            visible: root.docName !== ""

            Text {
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                anchors.verticalCenter: parent.verticalCenter
                text: (root.docDirty ? "• " : "") + root.docName
                color: root.docDirty ? Theme.fg : Theme.fgDim
                font.pixelSize: 12
                elide: Text.ElideRight
                width: Math.min(implicitWidth, 320)
            }

            HoverHandler { id: docHover }

            Controls.ToolTip {
                id: docTip
                text: root.docPath
                visible: docHover.hovered && root.docPath !== ""
                delay: 400
                padding: 0
                width: docTipText.implicitWidth + 16
                height: docTipText.implicitHeight + 8
                x: 0
                y: docLabel.height + 8
                background: Rectangle {
                    color: Theme.bgElev
                    border.width: 1
                    border.color: Theme.border
                    radius: 5
                }
                contentItem: Text {
                    id: docTipText
                    text: docTip.text
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2

            Repeater {
                model: ["文件", "编辑", "视图", "运行", "帮助"]

                delegate: Rectangle {
                    required property string modelData

                    width: menuLabel.implicitWidth + 20
                    height: 26
                    radius: 6
                    color: menuHover.hovered ? Theme.bgHover : "transparent"

                    HoverHandler { id: menuHover }

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        id: menuLabel
                        anchors.centerIn: parent
                        text: parent.modelData
                        color: menuHover.hovered ? Theme.fgBright : Theme.fg
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: menuArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData === "文件")
                                fileMenu.popup()
                            else if (modelData === "编辑")
                                editMenu.popup()
                            else if (modelData === "帮助")
                                helpMenu.popup()
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: undoRow.implicitWidth + 8
            height: 32
            radius: 8
            color: Theme.bg
            border.width: 1
            border.color: Theme.border

            Row {
                id: undoRow
                anchors.centerIn: parent
                spacing: 2

                TbButton {
                    glyph: "↺"
                    tooltip: "撤销 (Ctrl+Z)"
                    onActivated: root.undoRequested()
                }

                TbButton {
                    glyph: "↻"
                    tooltip: "重做 (Ctrl+Y)"
                    onActivated: root.redoRequested()
                }
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: viewRow.implicitWidth + 8
            height: 32
            radius: 8
            color: Theme.bg
            border.width: 1
            border.color: Theme.border

            Row {
                id: viewRow
                anchors.centerIn: parent
                spacing: 2

                TbButton {
                    glyph: "⤢"
                    tooltip: "适应视图 (Ctrl+0)"
                    onActivated: root.fitRequested()
                }

                TbButton {
                    glyph: "▦"
                    tooltip: "画布背景"
                    active: Settings.backgroundMode !== "none"
                    onActivated: {
                        // 循环：空白 → 点阵 → 网格 → 空白
                        Settings.backgroundMode = Settings.backgroundMode === "none" ? "dots"
                                                : Settings.backgroundMode === "dots" ? "grid" : "none"
                    }
                }

                TbButton {
                    glyph: "⌗"
                    tooltip: "网格吸附"
                    active: Settings.snapToGrid
                    onActivated: {
                        Settings.snapToGrid = !Settings.snapToGrid
                        UiBus.snapEnabled = Settings.snapToGrid
                    }
                }

                TbButton {
                    glyph: Theme.dark ? "☀" : "☾"
                    tooltip: "切换主题"
                    onActivated: Theme.toggle()
                }

                TbButton {
                    glyph: "⚙"
                    tooltip: "设置"
                    onActivated: UiBus.settingsRequested()
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Pill {
            anchors.verticalCenter: parent.verticalCenter
            label: "清空画布"
            onActivated: root.clearRequested()
        }

        Pill {
            anchors.verticalCenter: parent.verticalCenter
            label: NodeManager.engineRunning ? "停止" : "运行"
            dot: true
            primary: !NodeManager.engineRunning
            danger: NodeManager.engineRunning
            tooltip: NodeManager.engineRunning ? "停止当前运行" : "运行节点图"
            onActivated: NodeManager.engineRunning ? NodeManager.cancelRun()
                                                   : NodeManager.run()
        }
    }

    Controls.Menu {
        id: fileMenu

        MenuRow { text: "新建"; onTriggered: root.newRequested() }
        MenuRow { text: "打开…"; onTriggered: root.openRequested() }
        MenuSep {}
        MenuRow { text: "保存"; onTriggered: root.saveRequested() }
        MenuRow { text: "另存为…"; onTriggered: root.saveAsRequested() }

        background: Rectangle {
            implicitWidth: 170
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border
            radius: 8
        }
    }

    Controls.Menu {
        id: editMenu

        MenuRow { text: "设置…"; onTriggered: UiBus.settingsRequested() }

        background: Rectangle {
            implicitWidth: 170
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border
            radius: 8
        }
    }

    Controls.Menu {
        id: helpMenu

        MenuRow { text: "打开日志"; onTriggered: Log.openFile() }
        MenuRow { text: "打开日志文件夹"; onTriggered: Log.openFolder() }
        MenuRow { text: "复制日志路径"; onTriggered: Log.copyPath() }

        background: Rectangle {
            implicitWidth: 170
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border
            radius: 8
        }
    }

    component TbButton: Rectangle {
        id: tb

        property string glyph: ""
        property bool active: false
        property string tooltip: ""

        signal activated()

        width: 30
        height: 26
        radius: 6
        color: tb.active
               ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, tbHover.hovered ? 0.24 : 0.14)
               : (tbHover.hovered ? Theme.bgHover : "transparent")
        Behavior on color { ColorAnimation { duration: 120 } }

        HoverHandler { id: tbHover }

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.centerIn: parent
            text: tb.glyph
            color: tb.active ? Theme.blue
                             : (tbHover.hovered ? Theme.fgBright : Theme.fgDim)
            font.pixelSize: 14
        }

        MouseArea {
            id: tbArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tb.activated()
        }

        Controls.ToolTip {
            id: tbTip
            text: tb.tooltip
            visible: tb.tooltip !== "" && tbHover.hovered
            delay: 400
            padding: 0
            width: tbTipText.implicitWidth + 16
            height: tbTipText.implicitHeight + 8
            x: Math.round((tb.width - width) / 2)
            y: tb.height + 10
            background: Rectangle {
                color: Theme.bgElev
                border.width: 1
                border.color: Theme.border
                radius: 5
            }
            contentItem: Text {
                id: tbTipText
                text: tbTip.text
                color: Theme.fg
                font.pixelSize: 11
                renderType: Text.NativeRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    component MenuRow: Controls.MenuItem {
        id: mr
        implicitWidth: 170
        implicitHeight: 30
        padding: 0
        contentItem: Text {
            text: mr.text
            color: mr.hovered ? Theme.fgBright : Theme.fg
            font.pixelSize: 12
            leftPadding: 12
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }
        background: Rectangle {
            radius: 6
            color: mr.hovered ? Theme.bgHover : "transparent"
        }
    }

    component MenuSep: Controls.MenuSeparator {
        implicitWidth: 170
        padding: 0
        topPadding: 4
        bottomPadding: 4
        contentItem: Rectangle {
            implicitWidth: 158
            implicitHeight: 1
            color: Theme.borderSoft
        }
    }

    component Pill: Rectangle {
        id: pill

        property string label: ""
        property string tooltip: ""
        property bool disabled: false
        property bool dot: false
        property bool primary: false
        property bool danger: false
        readonly property color accent: pill.danger ? Theme.red : Theme.success
        readonly property bool solid: (pill.primary || pill.danger) && !pill.disabled
        readonly property color onAccent: Theme.dark ? "#0d1117" : "#ffffff"

        signal activated()

        implicitWidth: contentRow.implicitWidth + 28
        implicitHeight: 30
        width: implicitWidth
        height: implicitHeight
        radius: 8
        color: pill.disabled
               ? Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b, 0.35)
               : pill.solid
                 ? (pillHover.hovered ? Qt.darker(pill.accent, 1.1) : pill.accent)
                 : (pillHover.hovered ? Theme.bgHover : Theme.bg)
        border.width: 1
        border.color: pill.disabled
                      ? Theme.border
                      : pill.solid
                        ? pill.accent
                        : (pillHover.hovered ? Theme.blue : Theme.border)

        Row {
            id: contentRow
            anchors.centerIn: parent
            spacing: 7

            Rectangle {
                visible: pill.dot
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 3.5
                color: pill.solid ? pill.onAccent : Theme.green
                opacity: 0.9
            }

            Text {
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                id: pillLabel
                anchors.verticalCenter: parent.verticalCenter
                text: pill.label
                color: pill.solid ? pill.onAccent : (pill.disabled ? Theme.fgDim : Theme.fg)
                font.pixelSize: 12
            }
        }

        HoverHandler { id: pillHover }

        MouseArea {
            id: pillArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: pill.disabled ? Qt.ForbiddenCursor : Qt.PointingHandCursor
            onClicked: if (!pill.disabled) pill.activated()
        }

        Controls.ToolTip {
            id: pillTip
            text: pill.tooltip
            visible: pill.tooltip !== "" && pillHover.hovered
            delay: 400
            padding: 0
            width: pillTipText.implicitWidth + 16
            height: pillTipText.implicitHeight + 8
            x: Math.round((pill.width - width) / 2)
            y: pill.height + 10
            background: Rectangle {
                color: Theme.bgElev
                border.width: 1
                border.color: Theme.border
                radius: 5
            }
            contentItem: Text {
                id: pillTipText
                text: pillTip.text
                color: Theme.fg
                font.pixelSize: 11
                renderType: Text.NativeRendering
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
