import Settings
import QtQuick
import Theme

Item {
    id: row

    property string title
    property string desc
    property string keywords
    property bool placeholder: false
    readonly property bool isRow: true

    default property alias rowContent: controlSlot.data

    width: parent ? parent.width : 0
    implicitHeight: Math.max(labelCol.implicitHeight, holder.height) + 22
    height: implicitHeight

    function matchesQuery(q) {
        return ((title + " " + desc + " " + keywords).toLowerCase().indexOf(q) >= 0)
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 4
        anchors.rightMargin: 4
        height: 1
        color: Theme.borderSoft
    }

    Column {
        id: labelCol
        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width - holder.width - 32
        spacing: 2

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            width: parent.width
            text: row.title
            color: Theme.fgBright
            font.pixelSize: 13
            font.bold: true
            wrapMode: Text.WordWrap
        }
        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            width: parent.width
            visible: row.desc !== ""
            text: row.desc
            color: Theme.fgDim
            font.pixelSize: 12
            lineHeight: 1.35
            wrapMode: Text.WordWrap
        }
    }

    Row {
        id: holder
        anchors.right: parent.right
        anchors.rightMargin: 4
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Badge {
            anchors.verticalCenter: parent.verticalCenter
            visible: row.placeholder
            text: "即将支持"
        }

        Item {
            id: controlSlot
            anchors.verticalCenter: parent.verticalCenter
            enabled: !row.placeholder
            width: childrenRect.width
            height: childrenRect.height
        }
    }
}
