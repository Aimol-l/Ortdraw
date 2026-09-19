import QtQuick
import Theme
import UiBus

// 点击节点缩略图后放大查看的浮层
Item {
    id: viewer
    anchors.fill: parent
    visible: src !== ""
    z: 200
    onVisibleChanged: UiBus.overlayOpen = visible

    property string src: ""

    function open(s) { src = "" + s }
    function close() { src = "" }

    // 半透明遮罩，点击空白处关闭
    Rectangle {
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, 0.55)
        MouseArea {
            anchors.fill: parent
            onClicked: viewer.close()
        }
    }

    Rectangle {
        id: panel
        anchors.centerIn: parent
        width: parent.width * 0.8
        height: parent.height * 0.8
        radius: 10
        color: Theme.bgPanel
        border.width: 1
        border.color: Theme.border
        // 阻止点击穿透到遮罩
        MouseArea { anchors.fill: parent }

        Image {
            id: img
            anchors.fill: parent
            anchors.margins: 12
            source: viewer.src
            fillMode: Image.PreserveAspectFit
            asynchronous: true
            smooth: true
        }

        // 关闭按钮
        Rectangle {
            width: 24
            height: 24
            radius: 12
            color: closeArea.containsMouse ? Theme.red : Theme.bgHover
            border.width: 1
            border.color: Theme.border
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.rightMargin: 6
            anchors.topMargin: 6

            Text {
                anchors.centerIn: parent
                text: "×"
                color: closeArea.containsMouse ? "#ffffff" : Theme.fg
                font.pixelSize: 16
                renderType: Text.CurveRendering
            }
            MouseArea {
                id: closeArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: viewer.close()
            }
        }
    }

    Shortcut {
        sequence: "Escape"
        enabled: viewer.visible
        onActivated: viewer.close()
    }

    Connections {
        target: UiBus
        function onPreviewRequested(s) { viewer.open(s) }
    }
}
