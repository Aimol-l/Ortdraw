import QtQuick
import QtQuick.Controls as Controls
import Theme
import UiBus
import Settings

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
                    text: "Ortdraw"
                    color: Theme.fgBright
                    font.pixelSize: 14
                    font.bold: true
                }

                Text {
                    text: "节点式图像处理"
                    color: Theme.fgDim
                    font.pixelSize: 10
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
            label: "运行"
            dot: true
            disabled: true
            tooltip: "执行引擎尚未实现"
        }
    }

    Controls.Menu {
        id: fileMenu

        Controls.MenuItem {
            text: "新建"
            onTriggered: root.newRequested()
        }

        Controls.MenuItem {
            text: "打开…"
            onTriggered: root.openRequested()
        }

        Controls.MenuSeparator {}

        Controls.MenuItem {
            text: "保存"
            onTriggered: root.saveRequested()
        }

        Controls.MenuItem {
            text: "另存为…"
            onTriggered: root.saveAsRequested()
        }

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

        Controls.MenuItem {
            text: "设置…"
            onTriggered: UiBus.settingsRequested()
        }

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

    component Pill: Rectangle {
        id: pill

        property string label: ""
        property string tooltip: ""
        property bool disabled: false
        property bool dot: false

        signal activated()

        implicitWidth: contentRow.implicitWidth + 28
        implicitHeight: 30
        width: implicitWidth
        height: implicitHeight
        radius: 8
        color: pill.disabled
               ? Qt.rgba(Theme.green.r, Theme.green.g, Theme.green.b, 0.35)
               : (pillHover.hovered ? Theme.bgHover : Theme.bg)
        border.width: 1
        border.color: pill.disabled
                      ? Theme.border
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
                color: Theme.green
                opacity: 0.9
            }

            Text {
                id: pillLabel
                anchors.verticalCenter: parent.verticalCenter
                text: pill.label
                color: pill.disabled ? Theme.fgDim : Theme.fg
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
