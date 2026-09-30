import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager
import FileDialogs

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

    // 最上层校验：把 item 上的点映射到“nodes/world”层，问该点最上层是不是本节点。
    // 不是（本节点被别的节点压住）就不执行本次动作，改为选中并置顶最上层节点，
    // 避免“隔着上层节点操作到下层控件”。
    function claimClick(item, p) {
        if (!node || !node.parent || !item) return true
        var w = item.mapToItem(node.parent, p.x, p.y)
        var top = NodeManager.topNodeUuidAt(w.x, w.y)
        if (top === "" || top === ("" + node.uuid)) return true
        NodeManager.bringToFront(top)
        NodeManager.mousePressEvent(Qt.point(w.x, w.y), false)
        return false
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
                id: boolSwitch
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
                TapHandler {
                    onTapped: (p) => {
                        if (field.claimClick(boolSwitch, p.position))
                            field.commit(!field.boolValue())
                    }
                }
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
        TapHandler {
            onTapped: (p) => {
                if (!field.claimClick(selectBox, p.position)) return
                selectMenu.visible ? selectMenu.close() : selectMenu.open()
            }
        }

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
                    // 用户编辑会打断 text 绑定，而 joinCells() 读的是 cellBox.cellText，
                    // 不同步回去就会把旧值提交给节点（改了不生效）
                    onTextEdited: cellBox.cellText = text
                    onActiveFocusChanged: {
                        if (activeFocus && !field.claimClick(cellIn,
                                           Qt.point(cellIn.width / 2, cellIn.height / 2)))
                            cellIn.focus = false
                    }
                    onEditingFinished: floatsRow.joinCells()
                }

                Connections {
                    target: field
                    function onRevisionChanged() {
                        const v = floatsRow.cells[cellBox.index] !== undefined
                                  ? ("" + floatsRow.cells[cellBox.index]).trim() : ""
                        if (!cellIn.activeFocus) cellIn.text = v
                        cellBox.cellText = v
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
        spacing: 3

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
            width: 38
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
                onActiveFocusChanged: {
                    if (activeFocus && !field.claimClick(wCell,
                                       Qt.point(wCell.width / 2, wCell.height / 2)))
                        wCell.focus = false
                }
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
            width: 38
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
                onActiveFocusChanged: {
                    if (activeFocus && !field.claimClick(hCell,
                                       Qt.point(hCell.width / 2, hCell.height / 2)))
                        hCell.focus = false
                }
                onEditingFinished: size2Row.commit2()
            }
        }
    }

    // ---- file：标签 + 路径显示 + 「浏览…」按钮（如类别文件）----
    Row {
        id: fileRow
        visible: field.kind === "file"
        width: field.width
        height: field.height
        spacing: 4

        // 依赖 revision：参数变化（选择文件）后重新求值
        readonly property string filePath: (field.revision, "" + field.val(field.key))

        Text {
            id: fileLabel
            height: fileRow.height
            verticalAlignment: Text.AlignVCenter
            text: field.labelText
            color: Theme.fgDim
            font.pixelSize: 10
            renderType: field.textRenderType
        }
        Rectangle {
            height: field.height
            width: Math.max(30, fileRow.width - fileLabel.implicitWidth
                           - browseBtn.width - fileRow.spacing * 2)
            radius: 5
            color: Theme.bg
            border.width: 1
            border.color: Theme.border

            Text {
                id: filePathText
                anchors.fill: parent
                anchors.leftMargin: 6
                anchors.rightMargin: 4
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideMiddle
                color: fileRow.filePath === "" ? Theme.fgDim : Theme.fg
                font.pixelSize: 10
                text: fileRow.filePath === "" ? "未选择" : fileRow.filePath.split("/").pop()
                renderType: field.textRenderType
            }
        }
        Rectangle {
            id: browseBtn
            width: browseLabel.implicitWidth + 14
            height: field.height
            radius: 5
            color: browseHover.hovered ? Theme.bgHover : Theme.bgElev
            border.width: 1
            border.color: browseHover.hovered ? Theme.blue : Theme.border

            HoverHandler { id: browseHover }

            Text {
                id: browseLabel
                anchors.centerIn: parent
                text: "浏览…"
                color: Theme.fg
                font.pixelSize: 10
                renderType: field.textRenderType
            }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (!field.claimClick(browseBtn,
                            Qt.point(browseBtn.width / 2, browseBtn.height / 2)))
                        return
                    var p = FileDialogs.openClassFile("" + field.val(field.key))
                    if (p !== "")
                        field.commit(p)
                }
            }
        }
    }

    // ---- int / float / text：内联标签 + 右对齐值 ----
    Rectangle {
        id: scalarBox
        visible: field.kind !== "bool" && field.kind !== "select"
                 && field.kind !== "floats" && field.kind !== "size2"
                 && field.kind !== "file"
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
            onActiveFocusChanged: {
                if (activeFocus && !field.claimClick(scalarIn,
                                   Qt.point(scalarIn.width / 2, scalarIn.height / 2)))
                    scalarIn.focus = false
            }
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
