pragma ComponentBehavior: Bound
import Settings
import QtQuick
import QtQuick.Controls
import Theme
import NodeManager
import NodeCatalog

Rectangle {
    id: root

    color: Theme.bgPanel

    signal collapseRequested()

    readonly property var node: NodeManager.selectedNode
    readonly property bool hasNode: node !== null && node !== undefined
    readonly property var edge: NodeManager.selectedEdge
    readonly property bool hasEdge: !hasNode && edge !== null && edge !== undefined
                                     && edge.from !== undefined

    readonly property color catColor: !hasNode ? Theme.fg
        : node.category === "input"  ? Theme.catInput
        : node.category === "math"   ? Theme.catMath
        : node.category === "output" ? Theme.catOutput
        : Theme.catProcess

    readonly property string catName: !hasNode ? ""
        : (NodeCatalog.categoryNames[node.category] || node.category)

    readonly property string typeBadge: hasNode ? node.typeName
        : (hasEdge ? "连线" : "—")

    function shortUid(u) {
        if (!u)
            return "?"
        return String(u).substring(0, 8)
    }

    Rectangle {
        anchors.left: parent.left
        width: 1
        height: parent.height
        color: Theme.border
    }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 42

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 3

            Rectangle {
                id: collapseBtn
                width: 24
                height: 24
                radius: 6
                anchors.verticalCenter: parent.verticalCenter
                color: collapseArea.containsMouse ? Theme.bgHover : "transparent"

                Text {
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                    anchors.centerIn: parent
                    text: "›"
                    color: collapseArea.containsMouse ? Theme.fgBright : Theme.fgDim
                    font.pixelSize: 14
                }

                MouseArea {
                    id: collapseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.collapseRequested()
                }
            }

            Text {
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                anchors.verticalCenter: parent.verticalCenter
                text: "属性"
                color: Theme.fgBright
                font.pixelSize: 13
                font.bold: true
            }
        }

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            width: badgeText.implicitWidth + 14
            height: 20
            radius: 5
            color: Qt.rgba(Theme.blue.r, Theme.blue.g, Theme.blue.b, 0.12)

            Text {
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                id: badgeText
                anchors.centerIn: parent
                text: root.typeBadge
                color: Theme.blue
                font.pixelSize: 10
                font.bold: true
                font.family: "monospace"
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.borderSoft
        }
    }

    ScrollView {
        id: scroll
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            width: scroll.availableWidth

            Item {
                width: parent.width
                height: 240
                visible: !root.hasNode && !root.hasEdge

                Column {
                    anchors.top: parent.top
                    anchors.topMargin: 52
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "◇"
                        color: Theme.fgDim
                        opacity: 0.4
                        font.pixelSize: 30
                    }

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "选择一个节点\n查看并编辑其属性"
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.fgDim
                        font.pixelSize: 12
                        lineHeight: 1.6
                    }
                }
            }

            Column {
                id: nodePane
                width: parent.width
                visible: root.hasNode

                Item { width: parent.width; height: 14 }

                Column {
                    id: nodeBody
                    x: 14
                    width: parent.width - 28
                    spacing: 0

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "名称"
                        color: Theme.fgDim
                        font.pixelSize: 11
                    }

                    Item { width: 1; height: 5 }

                    TextField {
                        id: nameField
                        width: nodeBody.width
                        height: 34
                        text: root.hasNode ? root.node.name : ""
                        placeholderText: "name"
                        placeholderTextColor: Theme.fgDim
                        color: Theme.fg
                        font.pixelSize: 12
                        leftPadding: 10
                        rightPadding: 10
                        selectByMouse: true
                        onEditingFinished: {
                            if (root.hasNode) {
                                root.node.name = text
                                NodeManager.commitNodeParams(root.node.uuid)
                            }
                        }
                        background: Rectangle {
                            color: Theme.bg
                            radius: 7
                            border.width: 1
                            border.color: nameField.activeFocus ? Theme.blue : Theme.border
                        }
                        onVisibleChanged: if (visible && root.hasNode) text = root.node.name
                        Connections {
                            target: NodeManager
                            function onSelectionChanged() {
                                nameField.text = root.hasNode ? root.node.name : ""
                            }
                        }
                    }

                    Item { width: 1; height: 13 }

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "类型"
                        color: Theme.fgDim
                        font.pixelSize: 11
                    }

                    Item { width: 1; height: 5 }

                    Rectangle {
                        width: nodeBody.width
                        height: 36
                        radius: 7
                        color: Theme.bg
                        border.width: 1
                        border.color: Theme.border

                        Rectangle {
                            id: typeDot
                            width: 8
                            height: 8
                            radius: 3
                            color: root.catColor
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.left: typeDot.right
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.hasNode ? root.node.typeName : ""
                            color: Theme.fg
                            font.pixelSize: 12
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.catName
                            color: Theme.fgDim
                            font.pixelSize: 11
                        }
                    }

                    SecTitle { label: "输入端口" }

                    Flow {
                        width: nodeBody.width
                        spacing: 6

                        Repeater {
                            model: root.hasNode ? root.node.inputPorts.length : 0

                            delegate: Rectangle {
                                required property int index
                                property var port: root.node ? root.node.inputPorts[index] : null

                                width: inChipText.implicitWidth + 18
                                height: 24
                                radius: 6
                                color: Qt.rgba(Theme.catInput.r, Theme.catInput.g, Theme.catInput.b, 0.12)
                                border.width: 1
                                border.color: Qt.rgba(Theme.catInput.r, Theme.catInput.g, Theme.catInput.b, 0.32)

                                Text {
                                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                                    id: inChipText
                                    anchors.centerIn: parent
                                    text: parent.port ? parent.port.name + " · " + parent.port.dataTypeName : ""
                                    color: Theme.catInput
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    HintCard {
                        visible: root.hasNode && root.node.inputPorts.length === 0
                        label: "无输入端口"
                    }

                    SecTitle { label: "输出端口" }

                    Flow {
                        width: nodeBody.width
                        spacing: 6

                        Repeater {
                            model: root.hasNode ? root.node.outputPorts.length : 0

                            delegate: Rectangle {
                                required property int index
                                property var port: root.node ? root.node.outputPorts[index] : null

                                width: outChipText.implicitWidth + 18
                                height: 24
                                radius: 6
                                color: Qt.rgba(Theme.catOutput.r, Theme.catOutput.g, Theme.catOutput.b, 0.12)
                                border.width: 1
                                border.color: Qt.rgba(Theme.catOutput.r, Theme.catOutput.g, Theme.catOutput.b, 0.32)

                                Text {
                                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                                    id: outChipText
                                    anchors.centerIn: parent
                                    text: parent.port ? parent.port.name + " · " + parent.port.dataTypeName : ""
                                    color: Theme.catOutput
                                    font.pixelSize: 11
                                }
                            }
                        }
                    }

                    HintCard {
                        visible: root.hasNode && root.node.outputPorts.length === 0
                        label: "无输出端口"
                    }

                    SecTitle { label: "参数" }

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "X / Y"
                        color: Theme.fgDim
                        font.pixelSize: 11
                    }

                    Item { width: 1; height: 5 }

                    Rectangle {
                        width: nodeBody.width
                        height: 36
                        radius: 7
                        color: Theme.bg
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "pos"
                            color: Theme.fgDim
                            font.family: "monospace"
                            font.pixelSize: 11
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.left: parent.left
                            anchors.leftMargin: 40
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.hasNode
                                  ? Math.round(root.node.x) + ", " + Math.round(root.node.y)
                                  : ""
                            color: Theme.fg
                            font.pixelSize: 12
                        }
                    }

                    SecTitle {
                        visible: root.hasNode && root.node.description.length > 0
                        label: "描述"
                    }

                    Rectangle {
                        visible: root.hasNode && root.node.description.length > 0
                        width: nodeBody.width
                        height: descText.implicitHeight + 18
                        radius: 8
                        color: Theme.bg
                        border.width: 1
                        border.color: Theme.borderSoft

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            id: descText
                            x: 10
                            y: 9
                            width: parent.width - 20
                            text: root.hasNode ? root.node.description : ""
                            color: Theme.fg
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    Item { width: 1; height: 18 }

                    Rectangle {
                        id: deleteNodeBtn
                        width: nodeBody.width
                        height: 34
                        radius: 8
                        color: deleteNodeArea.containsMouse
                               ? Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.16)
                               : Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.09)
                        border.width: 1
                        border.color: Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.35)

                        Behavior on color { ColorAnimation { duration: 120 } }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.centerIn: parent
                            text: "删除节点"
                            color: Theme.red
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: deleteNodeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: NodeManager.removeNode()
                        }
                    }
                }

                Item { width: parent.width; height: 20 }
            }

            Column {
                id: edgePane
                width: parent.width
                visible: root.hasEdge

                Item { width: parent.width; height: 14 }

                Column {
                    id: edgeBody
                    x: 14
                    width: parent.width - 28
                    spacing: 0

                    Text {
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "连接"
                        color: Theme.fgDim
                        font.pixelSize: 11
                    }

                    Item { width: 1; height: 5 }

                    Rectangle {
                        width: edgeBody.width
                        height: 36
                        radius: 7
                        color: Theme.bg
                        border.width: 1
                        border.color: Theme.border

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.shortUid(root.edge.from) + " #" + root.edge.fromPort
                            color: Theme.fg
                            font.family: "monospace"
                            font.pixelSize: 11
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.centerIn: parent
                            text: "→"
                            color: Theme.fgDim
                            font.pixelSize: 12
                        }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.right: parent.right
                            anchors.rightMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: "#" + root.edge.toPort + " " + root.shortUid(root.edge.to)
                            color: Theme.fg
                            font.family: "monospace"
                            font.pixelSize: 11
                        }
                    }

                    Item { width: 1; height: 13 }

                    HintCard {
                        label: "按 Delete 或点击下方按钮删除连线。"
                    }

                    Item { width: 1; height: 18 }

                    Rectangle {
                        id: deleteEdgeBtn
                        width: edgeBody.width
                        height: 34
                        radius: 8
                        color: deleteEdgeArea.containsMouse
                               ? Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.16)
                               : Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.09)
                        border.width: 1
                        border.color: Qt.rgba(Theme.red.r, Theme.red.g, Theme.red.b, 0.35)

                        Behavior on color { ColorAnimation { duration: 120 } }

                        Text {
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                            anchors.centerIn: parent
                            text: "删除连线"
                            color: Theme.red
                            font.pixelSize: 12
                            font.bold: true
                        }

                        MouseArea {
                            id: deleteEdgeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: NodeManager.removeEdge()
                        }
                    }
                }

                Item { width: parent.width; height: 20 }
            }
        }
    }

    component SecTitle: Item {
        property string label

        width: parent ? parent.width : 0
        height: 36

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 9
            text: parent.label
            color: Theme.fgDim
            font.pixelSize: 10
            font.bold: true
            font.letterSpacing: 0.9
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: Theme.borderSoft
        }
    }

    component HintCard: Rectangle {
        property string label

        width: parent ? parent.width : 0
        height: 34
        radius: 8
        color: Theme.bg
        border.width: 1
        border.color: Theme.borderSoft

        Text {
            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            anchors.left: parent.left
            anchors.leftMargin: 11
            anchors.right: parent.right
            anchors.rightMargin: 11
            anchors.verticalCenter: parent.verticalCenter
            text: parent.label
            color: Theme.fgDim
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
