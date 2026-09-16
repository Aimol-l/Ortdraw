import QtQuick
import Theme

Rectangle {
    id: root

    height: 30
    color: Theme.bgPanel

    property int nodeCount: 0
    property int edgeCount: 0
    property string selectedName: "无"
    property real zoom: 1.0

    readonly property string themeName: Theme.dark ? "暗色" : "亮色"

    signal zoomInRequested()
    signal zoomOutRequested()

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

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40
                    horizontalAlignment: Text.AlignHCenter
                    text: Math.round(root.zoom * 100) + "%"
                    color: Theme.fg
                font.pixelSize: 11
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
