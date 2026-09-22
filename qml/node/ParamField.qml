import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 单个任务参数控件（紧凑版）：标签内联到控件内部左侧（灰字，10px），
// 高度统一 20；宽度由父级（任务行/网格列）给定。
Item {
    id: field

    property var node
    property var desc
    property int revision: 0

    implicitHeight: 20
    height: implicitHeight

    readonly property string kind: desc ? desc.kind : ""
    readonly property string key: (desc && desc.key !== undefined) ? desc.key : ""
    readonly property string labelText: desc ? desc.label : ""
    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    function val(k) {
        if (!node || k === "") return ""
        var m = node.taskParams()
        return m[k] === undefined ? "" : m[k]
    }
    function boolValue() { return (field.revision, String(field.val(field.key)) === "true") }
    function optionLabel(options, v) {
        if (!options) return "" + v
        for (var i = 0; i < options.length; ++i)
            if ("" + options[i].value === "" + v) return options[i].label
        return "" + v
    }
    function commit(v) {
        if (!node || field.key === "") return
        node.setTaskParam(field.key, v)
        NodeManager.commitNodeParams(node.uuid)
    }

    // ---- bool：标签 + 开关（无外框，开关靠右）----
    Row {
        id: boolRow
        visible: field.kind === "bool"
        width: field.width
        height: field.height
        spacing: 6

        Text {
            width: Math.max(0, boolRow.width - boolToggleItem.width - boolRow.spacing)
            height: boolRow.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        Item {
            id: boolToggleItem
            width: 30
            height: boolRow.height
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 30
                height: 17
                radius: 8.5
                color: field.boolValue() ? Theme.blue : Theme.bg
                border.width: 1
                border.color: field.boolValue() ? Theme.blue : Theme.border
                Behavior on color { ColorAnimation { duration: 130 } }
                Rectangle {
                    width: 13
                    height: 13
                    radius: 6.5
                    y: 2
                    x: field.boolValue() ? 15 : 2
                    color: field.boolValue() ? "#ffffff" : Theme.fgDim
                    Behavior on x { NumberAnimation { duration: 130 } }
                }
                TapHandler { onTapped: field.commit(!field.boolValue()) }
            }
        }
    }

    // ---- select：标签 + 值 + ▾（一个内联框）----
    Rectangle {
        id: selectBox
        visible: field.kind === "select"
        width: field.width
        height: field.height
        radius: 5
        color: selectHover.hovered || selectMenu.visible ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            id: selectLabel
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, selectBox.width * 0.45))
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        Text {
            id: selectCaret
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Text {
            anchors.left: selectLabel.right
            anchors.leftMargin: 6
            anchors.right: selectCaret.left
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignRight
            text: (field.revision,
                   field.optionLabel(field.desc ? field.desc.options : null,
                                     field.val(field.key)))
            color: Theme.fg
            font.pixelSize: 11
            elide: Text.ElideRight
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

    // ---- floats：标签 + 每格 40px 输入框（逗号分隔存储）----
    Row {
        id: floatsRow
        visible: field.kind === "floats"
        width: field.width
        height: field.height
        spacing: 4

        readonly property var cells: ("" + field.val(field.key)).split(",")

        function joinCells() {
            var parts = []
            for (var i = 0; i < floatRepeater.count; ++i) {
                var it = floatRepeater.itemAt(i)
                if (it) parts.push(it.cellText)
            }
            field.commit(parts.join(","))
        }

        Text {
            height: floatsRow.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Repeater {
            id: floatRepeater
            model: field.desc ? field.desc.vecCount : 0
            delegate: Rectangle {
                id: cellBox
                required property int index
                property string cellText: floatsRow.cells[index] !== undefined
                                          ? ("" + floatsRow.cells[index]).trim() : ""
                width: 40
                height: field.height
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
                        if (!cellIn.activeFocus)
                            cellIn.text = floatsRow.cells[cellBox.index] !== undefined
                                          ? ("" + floatsRow.cells[cellBox.index]).trim() : ""
                    }
                }
            }
        }
    }

    // ---- size2：标签 + W × H（每格 44px，存储为 "WxH"）----
    Row {
        id: size2Row
        visible: field.kind === "size2"
        width: field.width
        height: field.height
        spacing: 4

        // 引用 revision 以在参数变化（如下拉切换）时重新求值
        readonly property var parts: (field.revision,
            ("" + field.val(field.key)).split("x"))

        function commit2() {
            field.commit(parseInt(wCell.text, 10) + "x" + parseInt(hCell.text, 10))
        }

        Text {
            height: size2Row.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Rectangle {
            width: 44
            height: field.height
            radius: 5
            color: Theme.bg
            border.width: 1
            border.color: wCell.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: wCell
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg
                font.pixelSize: 11
                selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 0 ? ("" + size2Row.parts[0]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
        Text {
            height: size2Row.height
            verticalAlignment: Text.AlignVCenter
            text: "×"
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Rectangle {
            width: 44
            height: field.height
            radius: 5
            color: Theme.bg
            border.width: 1
            border.color: hCell.activeFocus ? Theme.blue : Theme.border
            TextInput {
                id: hCell
                anchors.fill: parent
                horizontalAlignment: TextInput.AlignHCenter
                verticalAlignment: TextInput.AlignVCenter
                color: Theme.fg
                font.pixelSize: 11
                selectByMouse: true
                renderType: field.textRenderType
                text: size2Row.parts.length > 1 ? ("" + size2Row.parts[1]).trim() : ""
                onEditingFinished: size2Row.commit2()
            }
        }
    }

    // ---- int / float / text：内联标签 + 右对齐值 ----
    Rectangle {
        id: scalarBox
        visible: field.kind !== "bool" && field.kind !== "select"
                 && field.kind !== "floats" && field.kind !== "size2"
        width: field.width
        height: field.height
        radius: 5
        color: Theme.bg
        border.width: 1
        border.color: scalarIn.activeFocus ? Theme.blue : Theme.border

        Text {
            id: scalarLabel
            anchors.left: parent.left
            anchors.leftMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            width: Math.min(implicitWidth, Math.max(0, scalarBox.width - 22))
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            elide: Text.ElideRight
            renderType: field.textRenderType
        }
        TextInput {
            id: scalarIn
            anchors.left: scalarLabel.right
            anchors.leftMargin: 4
            anchors.right: parent.right
            anchors.rightMargin: 6
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: TextInput.AlignRight
            color: Theme.fg
            font.pixelSize: 11
            selectByMouse: true
            renderType: field.textRenderType
            text: (field.revision, "" + field.val(field.key))
            onEditingFinished: field.commit(text)
        }

        Connections {
            target: field
            function onRevisionChanged() {
                if (!scalarIn.activeFocus)
                    scalarIn.text = "" + field.val(field.key)
            }
        }
    }
}
