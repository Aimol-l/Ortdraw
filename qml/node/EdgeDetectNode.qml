import QtQuick
import EdgeDetectNode
import Theme
import Settings

EdgeDetectNode {
    id: root
    width: 220
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property var methodNames: ["Sobel", "Scharr", "Laplacian", "Canny"]
    readonly property bool kernelVisible: root.method !== 1   // Scharr 核固定为 3

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Column {
            width: parent.width
            spacing: 6

            Flow {
                width: parent.width
                spacing: 4

                Repeater {
                    model: root.methodNames
                    delegate: Rectangle {
                        required property int index
                        required property string modelData

                        width: chipLabel.implicitWidth + 14
                        height: 22
                        radius: 5
                        color: root.method === index ? Theme.blue : Theme.bg
                        border.width: 1
                        border.color: root.method === index ? Theme.blue : Theme.border

                        Text {
                            id: chipLabel
                            anchors.centerIn: parent
                            text: parent.modelData
                            color: root.method === index ? "#ffffff" : Theme.fgDim
                            font.pixelSize: 10
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.method = index
                        }
                    }
                }
            }

            Row {
                visible: root.kernelVisible
                spacing: 6

                Text {
                    height: 22
                    verticalAlignment: Text.AlignVCenter
                    text: root.method === 3 ? "光圈" : "核大小"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Repeater {
                    model: [3, 5, 7]
                    delegate: Rectangle {
                        required property int modelData

                        width: 26
                        height: 22
                        radius: 5
                        color: root.kernel === modelData ? Theme.blue : Theme.bg
                        border.width: 1
                        border.color: root.kernel === modelData ? Theme.blue : Theme.border

                        Text {
                            anchors.centerIn: parent
                            text: "" + parent.modelData
                            color: root.kernel === parent.modelData ? "#ffffff" : Theme.fgDim
                            font.pixelSize: 10
                            renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.kernel = parent.modelData
                        }
                    }
                }
            }

            Row {
                visible: root.method === 3
                spacing: 6

                Text {
                    height: 22
                    verticalAlignment: Text.AlignVCenter
                    text: "低"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Rectangle {
                    width: 46
                    height: 22
                    radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: lowIn.activeFocus ? Theme.blue : Theme.border

                    TextInput {
                        id: lowIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.low
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.low
                            root.low = v
                            text = "" + root.low
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!lowIn.activeFocus) lowIn.text = "" + root.low }
                        }
                    }
                }

                Text {
                    height: 22
                    verticalAlignment: Text.AlignVCenter
                    text: "高"
                    color: Theme.fg
                    font.pixelSize: 11
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }

                Rectangle {
                    width: 46
                    height: 22
                    radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: highIn.activeFocus ? Theme.blue : Theme.border

                    TextInput {
                        id: highIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                        text: "" + root.high
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.high
                            root.high = v
                            text = "" + root.high
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!highIn.activeFocus) highIn.text = "" + root.high }
                        }
                    }
                }
            }
        }
    }
}
