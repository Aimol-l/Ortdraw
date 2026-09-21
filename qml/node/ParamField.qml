import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 单个任务参数控件：按 ParamDesc.kind 生成（bool/select/floats/其它文本）
Row {
    id: field

    property var node
    property var desc
    property int revision: 0
    property int labelWidth: 46
    property int controlWidth: 90

    spacing: 6

    readonly property string kind: desc ? desc.kind : ""
    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    function val(key) {
        if (!node) return ""
        var m = node.taskParams()
        return m[key] === undefined ? "" : m[key]
    }
    function boolValue() { return String(val(desc.key)) === "true" }
    function optionLabel(options, v) {
        if (!options) return "" + v
        for (var i = 0; i < options.length; ++i)
            if ("" + options[i].value === "" + v) return options[i].label
        return "" + v
    }
    function commit(v) {
        if (!node) return
        node.setTaskParam(desc.key, v)
        NodeManager.commitNodeParams(node.uuid)
    }

    Text {
        anchors.verticalCenter: parent.verticalCenter
        visible: field.labelWidth > 0
        width: field.labelWidth
        text: field.desc ? field.desc.label : ""
        color: Theme.fgDim
        font.pixelSize: 11
        elide: Text.ElideRight
        renderType: field.textRenderType
    }

    // ---- bool ----
    Rectangle {
        id: boolBox
        visible: field.kind === "bool"
        anchors.verticalCenter: parent.verticalCenter
        width: 40
        height: 22
        radius: 11
        color: field.boolValue() ? Theme.blue : Theme.bg
        border.width: 1
        border.color: field.boolValue() ? Theme.blue : Theme.border
        Behavior on color { ColorAnimation { duration: 130 } }

        Rectangle {
            width: 16
            height: 16
            radius: 8
            y: 2
            x: field.boolValue() ? 21 : 2
            color: field.boolValue() ? "#ffffff" : Theme.fgDim
            Behavior on x { NumberAnimation { duration: 130 } }
        }

        TapHandler { onTapped: field.commit(!field.boolValue()) }
    }

    // ---- select ----
    Rectangle {
        id: selectBox
        visible: field.kind === "select"
        anchors.verticalCenter: parent.verticalCenter
        width: field.controlWidth
        height: 22
        radius: 5
        color: selectHover.hovered || selectMenu.visible ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            anchors.fill: parent
            anchors.leftMargin: 4
            anchors.rightMargin: 12
            verticalAlignment: Text.AlignVCenter
            text: (field.revision, field.optionLabel(field.desc ? field.desc.options : null,
                                                    field.val(field.desc ? field.desc.key : "")))
            color: Theme.fg
            font.pixelSize: 11
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 7
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.fgDim
            font.pixelSize: 10
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

    // ---- floats：按 vecCount 拆成多个输入框（逗号分隔存储） ----
    Row {
        id: floatsRow
        visible: field.kind === "floats"
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        readonly property var cells: ("" + field.val(field.desc ? field.desc.key : "")).split(",")

        function joinCells() {
            var parts = []
            for (var i = 0; i < floatRepeater.count; ++i) {
                var it = floatRepeater.itemAt(i)
                if (it) parts.push(it.cellText)
            }
            field.commit(parts.join(","))
        }

        Repeater {
            id: floatRepeater
            model: field.desc ? field.desc.vecCount : 0
            delegate: Rectangle {
                id: cellBox
                required property int index
                property string cellText: floatsRow.cells[index] !== undefined
                                          ? ("" + floatsRow.cells[index]).trim()
                                          : ""
                width: 52
                height: 22
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
                        if (!cellIn.activeFocus) {
                            cellIn.text = floatsRow.cells[cellBox.index] !== undefined
                                          ? ("" + floatsRow.cells[cellBox.index]).trim() : ""
                        }
                    }
                }
            }
        }
    }

    // ---- size2：宽x高（存储为 "WxH"） ----
    Row {
        id: size2Row
        visible: field.kind === "size2"
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4

        // 引用 revision 以在参数变化（如下拉切换）时重新求值
        readonly property var parts: (field.revision,
            ("" + field.val(field.desc ? field.desc.key : "")).split("x"))

        function commit2() {
            field.commit(parseInt(wIn.text, 10) + "x" + parseInt(hIn.text, 10))
        }

        Rectangle {
            width: 54; height: 22; radius: 5
            color: Theme.bg
            border.width: 1
            border.color: wIn.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: wIn
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg; font.pixelSize: 11; selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 0 ? ("" + size2Row.parts[0]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "×"; color: Theme.fgDim; font.pixelSize: 11
            renderType: field.textRenderType
        }
        Rectangle {
            width: 54; height: 22; radius: 5
            color: Theme.bg
            border.width: 1
            border.color: hIn.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: hIn
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg; font.pixelSize: 11; selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 1 ? ("" + size2Row.parts[1]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
    }

    // ---- int / float / text ----
    Rectangle {
        id: textBox
        visible: field.kind !== "bool" && field.kind !== "select"
                 && field.kind !== "floats" && field.kind !== "size2"
        anchors.verticalCenter: parent.verticalCenter
        width: field.controlWidth
        height: 22
        radius: 5
        color: Theme.bg
        border.width: 1
        border.color: textIn.activeFocus ? Theme.blue : Theme.border

        TextInput {
            id: textIn
            anchors.fill: parent
            horizontalAlignment: TextInput.AlignHCenter
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.fg
            font.pixelSize: 11
            selectByMouse: true
            renderType: field.textRenderType
            text: (field.revision, "" + field.val(field.desc ? field.desc.key : ""))
            onEditingFinished: field.commit(text)
        }

        Connections {
            target: field
            function onRevisionChanged() {
                if (!textIn.activeFocus)
                    textIn.text = "" + field.val(field.desc ? field.desc.key : "")
            }
        }
    }
}
