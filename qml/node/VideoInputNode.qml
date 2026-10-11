import QtQuick
import QtQuick.Controls
import VideoInputNode
import Theme
import Settings
import FileDialogs
import NodeManager

VideoInputNode {
    id: root
    width: 240
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
    // 播放状态（纯 UI 态，不进 params）
    property bool playing: false

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 4

            // ---- 选择文件 ----
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
                    Text {
                        id: pickLabel
                        anchors.centerIn: parent
                        text: "选择视频"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: root.textRenderType
                    }
                    HoverHandler { id: pickHover }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const p = FileDialogs.openVideo(root.path)
                            if (p !== "") {
                                root.path = p
                                NodeManager.commitNodeParams(root.uuid)
                                NodeManager.run()
                            }
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - pickBtn.width - 6)
                    text: root.path === "" ? "未选择" : root.path
                    color: Theme.fgDim
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    renderType: root.textRenderType
                }
            }

            // ---- 信息行 ----
            Text {
                width: parent.width
                text: root.infoText
                color: root.videoValid ? Theme.fgDim : Theme.red
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: root.textRenderType
            }

            // ---- 播放条 ----
            Row {
                width: parent.width
                spacing: 6

                Rectangle {
                    id: playBtn
                    width: 30
                    height: 24
                    radius: 6
                    enabled: root.videoValid
                    opacity: enabled ? 1 : 0.4
                    color: playHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: playHover.hovered ? Theme.blue : Theme.border
                    Text {
                        anchors.centerIn: parent
                        text: root.playing ? "⏸" : "▶"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: root.textRenderType
                    }
                    HoverHandler { id: playHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.playing = !root.playing
                    }
                }

                Slider {
                    id: frameSlider
                    width: Math.max(50, parent.width - playBtn.width - frameText.width
                                    - fpsBox.width - 4 * parent.spacing)
                    enabled: root.frameCount > 0
                    from: 0
                    to: Math.max(0, root.frameCount - 1)
                    value: root.frame
                    onMoved: {
                        root.playing = false
                        root.frame = Math.round(value)
                        NodeManager.run()
                    }
                }

                Text {
                    id: frameText
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.frameCount > 0
                          ? (root.frame + 1) + " / " + root.frameCount
                          : "" + (root.frame + 1)
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: root.textRenderType
                }

                Rectangle {
                    id: fpsBox
                    width: 38
                    height: 22
                    radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: fpsIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: fpsIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        renderType: root.textRenderType
                        text: "" + root.fps
                        onEditingFinished: {
                            const v = parseInt(text, 10)
                            if (isNaN(v)) {
                                text = "" + root.fps      // 非法输入回显当前值
                            } else {
                                root.fps = v
                                NodeManager.commitNodeParams(root.uuid)
                            }
                        }
                    }
                }
            }

            // ---- 循环开关 ----
            Row {
                width: parent.width
                spacing: 6
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "循环播放"
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: root.textRenderType
                }
                Rectangle {
                    width: 30
                    height: 17
                    radius: 8.5
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.loop ? Theme.blue : Theme.bg
                    border.width: 1
                    border.color: root.loop ? Theme.blue : Theme.border
                    Rectangle {
                        width: 13
                        height: 13
                        radius: 6.5
                        y: 2
                        x: root.loop ? 15 : 2
                        color: root.loop ? "#ffffff" : Theme.fgDim
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.loop = !root.loop
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }
        }
    }

    // 播放定时器：每帧推进并触发一次全图运行；上一帧还没跑完就跳过（不堆积）
    Timer {
        id: playTimer
        interval: Math.max(1, 1000 / Math.max(1, root.fps))
        repeat: true
        running: root.playing && root.videoValid
        onTriggered: {
            if (NodeManager.engineRunning) return
            const total = root.frameCount
            if (total > 0 && root.frame + 1 >= total) {
                if (root.loop) {
                    root.frame = 0
                } else {
                    root.playing = false
                    return
                }
            } else {
                root.frame = root.frame + 1
            }
            NodeManager.run()
        }
    }
}
