import QtQuick
import QtQuick.Controls
import OnnxInferNode
import Theme
import Settings
import NodeManager
import FileDialogs

OnnxInferNode {
    id: root
    width: 240
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    readonly property var deviceOptions: [
        { value: "auto", label: "自动" },
        { value: "cpu",  label: "CPU" },
        { value: "cuda", label: "CUDA" }
    ]

    function deviceLabel() {
        for (var i = 0; i < deviceOptions.length; ++i)
            if (deviceOptions[i].value === root.device) return deviceOptions[i].label
        return root.device
    }

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: ""

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 6

            // ---- 模型文件 ----
            Row {
                width: parent.width
                spacing: 6

                Rectangle {
                    id: pickBtn
                    anchors.verticalCenter: parent.verticalCenter
                    width: pickLabel.implicitWidth + 20
                    height: 24
                    radius: 6
                    color: pickHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: pickHover.hovered ? Theme.blue : Theme.border
                    Behavior on color { ColorAnimation { duration: 120 } }

                    HoverHandler { id: pickHover }

                    Text {
                        id: pickLabel
                        anchors.centerIn: parent
                        text: "选择"
                        color: Theme.fg
                        font.pixelSize: 11
                        renderType: root.textRenderType
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            var p = FileDialogs.openModel(root.modelPath)
                            if (p !== "")
                                root.modelPath = p
                        }
                    }
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: reloadLabel.implicitWidth + 16
                    height: 24
                    radius: 6
                    color: reloadHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: reloadHover.hovered ? Theme.blue : Theme.border
                    Behavior on color { ColorAnimation { duration: 120 } }

                    HoverHandler { id: reloadHover }

                    Text {
                        id: reloadLabel
                        anchors.centerIn: parent
                        text: "重载"
                        color: Theme.fg
                        font.pixelSize: 11
                        renderType: root.textRenderType
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.reloadModel()
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - pickBtn.width - reloadLabel.width
                                    - parent.spacing * 2)
                    text: root.modelPath === "" ? "未选择模型" : root.modelPath.split("/").pop()
                    color: Theme.fgDim
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                    renderType: root.textRenderType
                }
            }

            // ---- 设备 ----
            Row {
                width: parent.width
                spacing: 6

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40
                    text: "设备"
                    color: Theme.fgDim
                    font.pixelSize: 11
                    renderType: root.textRenderType
                }

                Rectangle {
                    id: deviceBox
                    anchors.verticalCenter: parent.verticalCenter
                    width: 110
                    height: 22
                    radius: 5
                    color: deviceHover.hovered || deviceMenu.visible ? Theme.bgHover : Theme.bg
                    border.width: 1
                    border.color: Theme.border

                    Text {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 18
                        verticalAlignment: Text.AlignVCenter
                        text: root.deviceLabel()
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

                    HoverHandler { id: deviceHover }
                    TapHandler {
                        onTapped: deviceMenu.visible ? deviceMenu.close() : deviceMenu.open()
                    }

                    Menu {
                        id: deviceMenu
                        y: -(height + 4)
                        background: Rectangle {
                            implicitWidth: 130
                            color: Theme.bgElev
                            border.width: 1
                            border.color: Theme.border
                            radius: 8
                        }
                        Instantiator {
                            model: root.deviceOptions
                            delegate: MenuItem {
                                id: devItem
                                required property var modelData
                                implicitHeight: 26
                                text: modelData.label
                                onTriggered: {
                                    root.device = modelData.value
                                    NodeManager.commitNodeParams(root.uuid)
                                }
                                contentItem: Text {
                                    text: devItem.text
                                    color: devItem.hovered ? Theme.fgBright : Theme.fg
                                    font.pixelSize: 11
                                    leftPadding: 12
                                    rightPadding: 12
                                    verticalAlignment: Text.AlignVCenter
                                    renderType: root.textRenderType
                                }
                                background: Rectangle {
                                    radius: 6
                                    color: devItem.hovered ? Theme.bgHover : "transparent"
                                }
                            }
                            onObjectAdded: (index, object) => deviceMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => deviceMenu.removeItem(object)
                        }
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    text: "线程"
                    color: Theme.fgDim
                    font.pixelSize: 11
                    renderType: root.textRenderType
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 50
                    height: 22
                    radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: threadIn.activeFocus ? Theme.blue : Theme.border

                    TextInput {
                        id: threadIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        renderType: root.textRenderType
                        text: "" + root.threads
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.threads
                            root.threads = v
                            text = "" + root.threads
                            NodeManager.commitNodeParams(root.uuid)
                        }
                        Connections {
                            target: root
                            function onParamsChanged() {
                                if (!threadIn.activeFocus)
                                    threadIn.text = "" + root.threads
                            }
                        }
                    }
                }
            }

            // ---- IO 概览 ----
            Text {
                visible: root.ioText !== ""
                width: parent.width
                text: root.ioText
                color: Theme.fgDim
                font.pixelSize: 10
                wrapMode: Text.Wrap
                renderType: root.textRenderType
            }

            // ---- 错误提示 ----
            Text {
                visible: root.modelError !== ""
                width: parent.width
                text: root.modelError
                color: Theme.red
                font.pixelSize: 10
                wrapMode: Text.Wrap
                renderType: root.textRenderType
            }
        }
    }
}
