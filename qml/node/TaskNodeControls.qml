import QtQuick
import QtQuick.Controls
import Theme
import Settings
import NodeManager

// 任务节点通用控件：任务下拉 + 按 paramDescs() 自动生成参数行。
// 用于 PreProcessNode / PostProcessNode。
Column {
    id: root

    property var node
    // 参数/任务变化时递增，用于强制重新求值（函数调用绑定不会自动刷新）
    property int revision: 0

    spacing: 6
    width: parent ? parent.width : implicitWidth

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    function taskLabel() {
        if (!node) return ""
        var opts = node.taskOptions()
        for (var i = 0; i < opts.length; ++i)
            if (opts[i].id === node.task) return opts[i].name
        return node.task
    }

    function paramValue(key) {
        if (!node) return ""
        var m = node.taskParams()
        return m[key] === undefined ? "" : m[key]
    }

    function boolValue(key) {
        return String(root.paramValue(key)) === "true"
    }

    function optionLabel(options, val) {
        if (!options) return "" + val
        for (var i = 0; i < options.length; ++i)
            if ("" + options[i].value === "" + val) return options[i].label
        return "" + val
    }

    Connections {
        target: root.node
        ignoreUnknownSignals: true
        function onParamsChanged() { root.revision++ }
        function onTaskPortsChanged() { root.revision++ }
    }

    // ---- 任务下拉 ----
    Rectangle {
        id: taskBox
        width: parent.width
        height: 24
        radius: 5
        color: taskHover.hovered || taskMenu.visible ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        Text {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 20
            verticalAlignment: Text.AlignVCenter
            text: root.node ? (root.revision, root.taskLabel()) : ""
            color: Theme.fg
            font.pixelSize: 11
            elide: Text.ElideRight
            renderType: root.textRenderType
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: "▾"
            color: Theme.fgDim
            font.pixelSize: 11
            renderType: root.textRenderType
        }

        HoverHandler { id: taskHover }
        TapHandler { onTapped: taskMenu.visible ? taskMenu.close() : taskMenu.open() }

        Menu {
            id: taskMenu
            y: -(height + 6)
            background: Rectangle {
                implicitWidth: 200
                color: Theme.bgElev
                border.width: 1
                border.color: Theme.border
                radius: 8
            }
            Instantiator {
                model: root.node ? root.node.taskOptions() : []
                delegate: MenuItem {
                    id: taskItem
                    required property var modelData
                    implicitHeight: 28
                    text: modelData.name
                    onTriggered: {
                        if (root.node) {
                            root.node.task = modelData.id
                            NodeManager.commitNodeParams(root.node.uuid)
                        }
                    }
                    contentItem: Text {
                        text: taskItem.text
                        color: taskItem.hovered ? Theme.fgBright : Theme.fg
                        font.pixelSize: 11
                        leftPadding: 12
                        rightPadding: 12
                        verticalAlignment: Text.AlignVCenter
                        renderType: root.textRenderType
                    }
                    background: Rectangle {
                        radius: 6
                        color: taskItem.hovered ? Theme.bgHover : "transparent"
                    }
                }
                onObjectAdded: (index, object) => taskMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => taskMenu.removeItem(object)
            }
        }
    }

    // ---- 参数行：按 ParamDesc.kind 生成控件 ----
    Repeater {
        id: paramRepeater
        model: root.node ? (root.revision, root.node.paramDescs()) : []

        delegate: Row {
            id: paramRow
            required property var modelData

            width: root.width
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: 62
                text: paramRow.modelData.label
                color: Theme.fgDim
                font.pixelSize: 11
                elide: Text.ElideRight
                renderType: root.textRenderType
            }

            // bool：点击切换的开关
            Rectangle {
                id: boolBox
                visible: paramRow.modelData.kind === "bool"
                anchors.verticalCenter: parent.verticalCenter
                width: 40
                height: 22
                radius: 11
                color: root.boolValue(paramRow.modelData.key) ? Theme.blue : Theme.bg
                border.width: 1
                border.color: root.boolValue(paramRow.modelData.key) ? Theme.blue : Theme.border
                Behavior on color { ColorAnimation { duration: 130 } }

                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    y: 2
                    x: root.boolValue(paramRow.modelData.key) ? 21 : 2
                    color: root.boolValue(paramRow.modelData.key) ? "#ffffff" : Theme.fgDim
                    Behavior on x { NumberAnimation { duration: 130 } }
                }

                TapHandler {
                    onTapped: {
                        if (!root.node) return
                        root.node.setTaskParam(paramRow.modelData.key,
                                               !root.boolValue(paramRow.modelData.key))
                        NodeManager.commitNodeParams(root.node.uuid)
                    }
                }
            }

            // select：深色主题化下拉
            Rectangle {
                id: selectBox
                visible: paramRow.modelData.kind === "select"
                anchors.verticalCenter: parent.verticalCenter
                width: 110
                height: 22
                radius: 5
                color: selectHover.hovered || selectMenu.visible ? Theme.bgHover : Theme.bg
                border.width: 1
                border.color: Theme.border

                Text {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 18
                    verticalAlignment: Text.AlignVCenter
                    text: root.optionLabel(paramRow.modelData.options,
                                           root.paramValue(paramRow.modelData.key))
                    color: Theme.fg
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    renderType: root.textRenderType
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    text: "▾"
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: root.textRenderType
                }

                HoverHandler { id: selectHover }
                TapHandler {
                    onTapped: selectMenu.visible ? selectMenu.close() : selectMenu.open()
                }

                Menu {
                    id: selectMenu
                    y: -(height + 4)
                    background: Rectangle {
                        implicitWidth: 150
                        color: Theme.bgElev
                        border.width: 1
                        border.color: Theme.border
                        radius: 8
                    }
                    Instantiator {
                        model: paramRow.modelData.options
                        delegate: MenuItem {
                            id: optItem
                            required property var modelData
                            implicitHeight: 26
                            text: modelData.label
                            onTriggered: {
                                if (root.node) {
                                    root.node.setTaskParam(paramRow.modelData.key,
                                                           modelData.value)
                                    NodeManager.commitNodeParams(root.node.uuid)
                                }
                            }
                            contentItem: Text {
                                text: optItem.text
                                color: optItem.hovered ? Theme.fgBright : Theme.fg
                                font.pixelSize: 11
                                leftPadding: 12
                                rightPadding: 12
                                verticalAlignment: Text.AlignVCenter
                                renderType: root.textRenderType
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

            // int / float / text：文本输入，提交时交给 C++ 转换
            Rectangle {
                id: textBox
                visible: paramRow.modelData.kind !== "bool" && paramRow.modelData.kind !== "select"
                anchors.verticalCenter: parent.verticalCenter
                width: 90
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
                    renderType: root.textRenderType
                    text: "" + root.paramValue(paramRow.modelData.key)
                    onEditingFinished: {
                        if (!root.node) return
                        root.node.setTaskParam(paramRow.modelData.key, text)
                        NodeManager.commitNodeParams(root.node.uuid)
                    }
                }

                Connections {
                    target: root
                    function onRevisionChanged() {
                        if (!textIn.activeFocus)
                            textIn.text = "" + root.paramValue(paramRow.modelData.key)
                    }
                }
            }
        }
    }
}
