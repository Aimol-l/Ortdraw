import QtQuick
import ImageShowNode
import NodeManager

ImageShowNode{
    id:root
    width:220
    height:300
    property point dragStartPos: "0,0"
    Rectangle{
        id: node
        x: 2
        y: 2
        width:parent.width - 4
        height: parent.height - 4
        Column {
            spacing: 5
            Rectangle{
                height: 28
                width:node.width
                border.color: "#eff1f4" 
                border.width: 1 
                Text {
                    color: "gray"
                    font.pointSize: 17
                    text: root.name
                    anchors.centerIn: parent
                }
                MouseArea{
                    anchors.fill: parent
                    hoverEnabled: true
                    // propagateComposedEvents: true // 允许事件传播到父级
                    acceptedButtons: Qt.LeftButton
                    cursorShape: containsPress ? Qt.ClosedHandCursor : Qt.ArrowCursor
                    onPressed:(event)=> {
                        dragStartPos = Qt.point(event.x, event.y)
                        root.parent.close_dragging()
                    }
                    onReleased:(event)=> {
                        root.parent.open_dragging()
                        // root.selected = false
                    }
                    onClicked:(event)=>{
                        var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                        NodeManager.clickNodeEvent(root.uuid, ctrl)
                    }
                    onPositionChanged:(event)=>{
                        if (pressed) {
                            // 计算位移增量
                            let deltaX = event.x - dragStartPos.x
                            let deltaY = event.y - dragStartPos.y
                            // 更新组件位置（需要确保父级允许子项移动）
                            root.x += deltaX
                            root.y += deltaY
                            // 通知后端这个节点移动了，要更新Connection
                            NodeManager.nodeMoveEvent(root.uuid,deltaX,deltaY)
                        }
                    }
                }
            } 
        }
        // 动态生成输入端口
        Repeater {
            model: root.inputPorts
            delegate: 
                Rectangle {
                    height: 30
                    x: 5
                    y: 35 + index * (height + 5) // 每个端口间隔 5 像素
                    width:node.width/2
                    Row {
                        spacing: 5
                        Rectangle {
                            width: 18
                            height: 18
                            radius: 9 
                            color: "#17b9b9"
                            anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                            MouseArea{
                                anchors.fill: parent
                                hoverEnabled: true
                                propagateComposedEvents: true // 允许事件传播到父级
                                onEntered:{
                                    parent.width = 20
                                    parent.height = 20
                                    parent.radius = 10
                                }                                
                                onExited:{
                                    parent.width = 18
                                    parent.height = 18
                                    parent.radius = 9
                                }
                                onClicked:(event)=>{
                                    let x = mapToItem(root.parent,9,9).x
                                    let y = mapToItem(root.parent,9,9).y
                                    var port = modelData.self
                                    NodeManager.setInputPort(port,x,y)
                                }
                                Component.onCompleted: {
                                    var p = mapToItem(root.parent, width / 2, height / 2)
                                    root.setInputPortPosition(index, p.x, p.y)
                                }
                            }
                        }
                        Text{
                            color: "gray"
                            font.pointSize: 15
                            text: modelData.name
                            anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                        }
                    }
                }
        }
        // 动态生成输出端口
        Repeater {
            model: root.outputPorts
            delegate: 
                Rectangle {
                    height: 30
                    x:node.width/2
                    y: 35 + index * (height + 5) // 每个端口间隔 5 像素
                    width:node.width/2 - 5
                    Row {
                        spacing: 5
                        anchors.right: parent.right  // 将 Row 右对齐
                        Text{
                            color: "gray"
                            font.pointSize: 15
                            text: modelData.name
                            anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                        }
                        Rectangle {
                            width: 18
                            height: 18
                            radius: 9 
                            color: "#fe3521"
                            anchors.verticalCenter: parent.verticalCenter  // 垂直居中
                            MouseArea{
                                anchors.fill: parent
                                hoverEnabled: true
                                propagateComposedEvents: true // 允许事件传播到父级
                                onEntered:{
                                    parent.width = 20
                                    parent.height = 20
                                    parent.radius = 10
                                }
                                onExited:{
                                    parent.width = 18
                                    parent.height = 18
                                    parent.radius = 9
                                }
                                onClicked:(event)=>{
                                    let x = mapToItem(root.parent,9,9).x
                                    let y = mapToItem(root.parent,9,9).y

                                    var port = modelData.self
                                    NodeManager.setOutputPort(port,x,y)
                                }
                                Component.onCompleted: {
                                    var p = mapToItem(root.parent, width / 2, height / 2)
                                    root.setOutputPortPosition(index, p.x, p.y)
                                }
                            }
                        }
                    }
                }
        }
        MouseArea {
            width: 20
            height: 20
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            cursorShape: Qt.SizeFDiagCursor
            property real startX
            property real startY
            onPressed:(event)=> {
                startX = event.x
                startY = event.y
                root.parent.close_dragging()
            }
            onReleased:(event)=> {
                root.parent.open_dragging()
            }
            onPositionChanged: (event)=>{
                if (pressed) {
                    let deltaX = event.x - startX
                    let deltaY = event.y - startY
                    let new_w = Math.max(root.getMinWidth(), root.width + deltaX)
                    let new_h = Math.max(root.getMinHeight(), root.height + deltaY)
                    let dw = new_w - root.width
                    root.width = new_w
                    root.height = new_h
                    NodeManager.nodeResizeEvent(root.uuid, dw, 0)
                }
            }
        }
    }
}