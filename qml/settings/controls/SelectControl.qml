import QtQuick
import QtQuick.Controls
import Theme

Rectangle {
    id: sel

    property var options: []
    property var value
    signal picked(var v)

    function labelFor(v) {
        for (var i = 0; i < options.length; i++)
            if (String(options[i].value) === String(v)) return options[i].label
        return "" + v
    }

    implicitWidth: Math.max(150, labelText.implicitWidth + 40)
    implicitHeight: 30
    radius: 7
    color: Theme.bg
    border.width: 1
    border.color: Theme.border

    Text {
        id: labelText
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: sel.labelFor(sel.value)
        color: Theme.fg
        font.pixelSize: 12
    }
    Text {
        anchors.right: parent.right
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Theme.fgDim
        font.pixelSize: 11
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: menu.popup(sel, 0, sel.height + 4)
    }

    Menu {
        id: menu

        Repeater {
            model: sel.options
            delegate: MenuItem {
                required property var modelData
                text: modelData.label
                onTriggered: sel.picked(modelData.value)
            }
        }

        background: Rectangle {
            implicitWidth: 160
            color: Theme.bgElev
            border.width: 1
            border.color: Theme.border
            radius: 8
        }
    }
}
