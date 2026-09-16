import QtQuick
import QtQuick.Controls
import Theme

Rectangle {
    id: root

    height: 30
    color: Theme.bgPanel

    property int nodeCount: 0
    property int edgeCount: 0
    property string selectedName: "无"
    property string selectedPos: "-"
    property real zoom: 1.0

    readonly property string themeName: Theme.dark ? "暗色" : "亮色"
    readonly property var zoomPresets: [0.5, 0.75, 1.0, 1.25, 1.5, 2.0, 4.0]

    signal zoomInRequested()
    signal zoomOutRequested()
    signal zoomSetRequested(real z)
    signal fitRequested()

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 7
                height: 7
                radius: 3.5
                color: Theme.green
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "就绪"
                color: Theme.fgDim
                font.pixelSize: 11
            }
        }

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

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "坐标[" + root.selectedPos + "]"
            color: Theme.fgDim
            font.pixelSize: 11
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "主题 " + root.themeName
            color: Theme.fgDim
            font.pixelSize: 11
        }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "选中 " + root.selectedName
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
