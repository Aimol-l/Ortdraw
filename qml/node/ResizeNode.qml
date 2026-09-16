import QtQuick
import ResizeNode
import Theme

ResizeNode {
    id: root
    width: 220
    height: 300

    NodeCard {
        anchors.fill: parent
        node: root
        coordItem: root.parent

        Column {
            width: parent.width
            spacing: 8

            Row {
                spacing: 6
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: "输入尺寸"
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: Text.CurveRendering
                }
                Text {
                    height: 18
                    verticalAlignment: Text.AlignVCenter
                    text: root.inputSizeText
                    color: Theme.fg
                    font.pixelSize: 10
                    renderType: Text.CurveRendering
                }
            }

            Row {
                spacing: 6
                Repeater {
                    model: [{ t: "按尺寸", m: 0 }, { t: "按百分比", m: 1 }]
                    delegate: Rectangle {
                        required property var modelData
                        width: 68
                        height: 22
                        radius: 6
                        color: root.mode === modelData.m ? Theme.blue : "transparent"
                        border.width: 1
                        border.color: root.mode === modelData.m ? Theme.blue : Theme.border
                        Text {
                            anchors.centerIn: parent
                            text: modelData.t
                            color: root.mode === modelData.m ? "#ffffff" : Theme.fgDim
                            font.pixelSize: 10
                            renderType: Text.CurveRendering
                        }
                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.mode = modelData.m
                        }
                    }
                }
            }

            Row {
                spacing: 6
                visible: root.mode === 0

                Text {
                    height: 22; verticalAlignment: Text.AlignVCenter
                    text: "宽"; color: Theme.fg; font.pixelSize: 11; renderType: Text.CurveRendering
                }
                Rectangle {
                    width: 50; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: wIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: wIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        text: "" + root.outWidth
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.outWidth
                            root.outWidth = v
                            text = "" + root.outWidth
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!wIn.activeFocus) wIn.text = "" + root.outWidth }
                        }
                    }
                }

                Text {
                    height: 22; verticalAlignment: Text.AlignVCenter
                    text: "高"; color: Theme.fg; font.pixelSize: 11; renderType: Text.CurveRendering
                }
                Rectangle {
                    width: 50; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: hIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: hIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        text: "" + root.outHeight
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.outHeight
                            root.outHeight = v
                            text = "" + root.outHeight
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!hIn.activeFocus) hIn.text = "" + root.outHeight }
                        }
                    }
                }
            }

            Row {
                spacing: 6
                visible: root.mode === 1

                Text {
                    height: 22; verticalAlignment: Text.AlignVCenter
                    text: "比例"; color: Theme.fg; font.pixelSize: 11; renderType: Text.CurveRendering
                }
                Rectangle {
                    width: 58; height: 22; radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: pIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: pIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        text: "" + root.percent
                        onEditingFinished: {
                            var v = parseInt(text, 10)
                            if (isNaN(v)) v = root.percent
                            root.percent = v
                            text = "" + root.percent
                        }
                        Connections {
                            target: root
                            function onParamsChanged() { if (!pIn.activeFocus) pIn.text = "" + root.percent }
                        }
                    }
                }
                Text {
                    height: 22; verticalAlignment: Text.AlignVCenter
                    text: "%"; color: Theme.fgDim; font.pixelSize: 11; renderType: Text.CurveRendering
                }
            }
        }
    }
}
