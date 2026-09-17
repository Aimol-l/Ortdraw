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
                    color: menuArea.containsMouse ? Theme.bgHover : "transparent"

                    Text {
                        id: menuLabel
                        anchors.centerIn: parent
                        text: parent.modelData
                        color: menuArea.containsMouse ? Theme.fgBright : Theme.fg
                        font.pixelSize: 13
                    }

                    MouseArea {
                        id: menuArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            if (modelData === "编辑")
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
                    onActivated: root.undoRequested()
                }

                TbButton {
                    glyph: "↻"
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
                    onActivated: root.fitRequested()
                }

                TbButton {
                    glyph: "▦"
                    active: Settings.backgroundMode !== "none"
                    onActivated: {
                        // 循环：空白 → 点阵 → 网格 → 空白
                        Settings.backgroundMode = Settings.backgroundMode === "none" ? "dots"
                                                : Settings.backgroundMode === "dots" ? "grid" : "none"
                    }
                }

                TbButton {
                    glyph: "⌗"
                    active: Settings.snapToGrid
                    onActivated: {
                        Settings.snapToGrid = !Settings.snapToGrid
                        UiBus.snapEnabled = Settings.snapToGrid
                    }
                }

                TbButton {
                    glyph: Theme.dark ? "☀" : "☾"
                    onActivated: Theme.toggle()
                }

                TbButton {
                    glyph: "⚙"
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
               ? Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.14)
               : (tbArea.containsMouse ? Theme.bgHover : "transparent")
        Behavior on color { ColorAnimation { duration: 120 } }

        Text {
            anchors.centerIn: parent
            text: tb.glyph
            color: tb.active ? Theme.blue
                             : (tbArea.containsMouse ? Theme.fgBright : Theme.fgDim)
            font.pixelSize: 14
        }

        MouseArea {
            id: tbArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: tb.activated()
        }

        Rectangle {
            visible: tb.tooltip !== "" && tbArea.containsMouse
            z: 100
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
            anchors.topMargin: 6
            width: tbTip.implicitWidth + 16
            height: 24
            radius: 5
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border

            Text {
                id: tbTip
                anchors.centerIn: parent
                text: tb.tooltip
                color: Theme.fg
                font.pixelSize: 11
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
               : (pillArea.containsMouse ? Theme.bgHover : Theme.bg)
        border.width: 1
        border.color: pill.disabled
                      ? Theme.border
                      : (pillArea.containsMouse ? Theme.blue : Theme.border)

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

        MouseArea {
            id: pillArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: pill.disabled ? Qt.ForbiddenCursor : Qt.PointingHandCursor
            onClicked: if (!pill.disabled) pill.activated()
        }

        Rectangle {
            id: tipBox
            visible: pill.tooltip !== "" && pillArea.containsMouse
            z: 100
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom
            anchors.topMargin: 6
            width: tipLabel.implicitWidth + 16
            height: 24
            radius: 5
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border

            Text {
                id: tipLabel
                anchors.centerIn: parent
                text: pill.tooltip
                color: Theme.fg
                font.pixelSize: 11
            }
        }
    }
}
