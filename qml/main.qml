import QtQuick
import QtQuick.Window
import QtQuick.Controls 
import PaintBoard
import NodeManager

ApplicationWindow {
    width: 1600
    height: 900
    x: (Screen.width - width) / 2
    y: (Screen.height - height) / 2
    visible: true
    minimumWidth: 1280
    minimumHeight: 720
    title: "Ortdraw"
    // 节点绘制区域
    Rectangle{
        id: nodeArea
        height: parent.height
        width:  parent.width - left_side_bar.width
        anchors.left:left_side_bar.right
        focus: true // 确保这个矩形可以接收焦点
        property int maxVal: 4096
        property int currValX: Screen.width
        property int currValY: Screen.height
        // 画布
        Flickable {
            id: flickview
            clip: true
            anchors.fill: parent
            contentWidth:  nodeArea.currValX
            contentHeight: nodeArea.currValY
            boundsBehavior: Flickable.StopAtBounds
            // 动态改变画布大小
            onContentXChanged: checkBoundaries()
            onContentYChanged: checkBoundaries()
            function checkBoundaries() {
                const margin = 100 // 边界扩展阈值
                if (contentX + width > contentWidth - margin && flickview.contentWidth < nodeArea.maxVal) {
                    flickview.contentWidth += 200
                }
                if (contentY + height > contentHeight - margin &&  flickview.contentHeight < nodeArea.maxVal) {
                    flickview.contentHeight += 200
                }
            }
            // 需要绘制背景网格
            Canvas {
                width: flickview.contentWidth
                height: flickview.contentHeight
                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#ccc"
                    ctx.lineWidth = 1
                    for (var x = 0; x < width; x += 50) {
                        ctx.beginPath()
                        ctx.moveTo(x, 0)
                        ctx.lineTo(x, height)
                        ctx.stroke()
                    }
                    for (var y = 0; y < height; y += 50) {
                        ctx.beginPath()
                        ctx.moveTo(0, y)
                        ctx.lineTo(width, y)
                        ctx.stroke()
                    }
                }
            }
            MouseArea {
                id:mouseArea
                anchors.fill: parent
                hoverEnabled: true
                // 缩放控制参数
                property real scaleFactor: 1.0
                property real minScale: 0.3
                property real maxScale: 5.0
                onClicked: (event)=>{
                    var pos = Qt.point(event.x, event.y);
                    var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                    NodeManager.mousePressEvent(pos, ctrl)
                }
                onPressed:(event)=>{
                    cursorShape = Qt.SizeAllCursor
                }
                onReleased:(event)=>{
                    cursorShape = Qt.ArrowCursor
                }
                onPositionChanged: (event)=>{
                    NodeManager.mouseMoveEvent(event.x, event.y)
                }
                onWheel: (wheel)=>{
                    wheel.accepted = true
                }
            }
            // 绘制节点之间的连线
            PaintBoard {
                id: paint_board
                anchors.fill: parent
            }
            // 绘制节点
            Item {
                id: canvas
                anchors.fill: parent
                function close_dragging(){
                    flickview.interactive = false
                    // console.log("close")
                }
                function open_dragging(){
                    flickview.interactive = true
                    // console.log("open")
                }
            }
            Component.onCompleted: {
                NodeManager.setPaintBoard(paint_board)
            }
            onMovementStarted:{
                console.log("start")
            }
            onMovementEnded:{
                mouseArea.cursorShape = Qt.ArrowCursor
                console.log("stop")
            }
           
        }
        Keys.onPressed: (event)=> {
            if (event.key == Qt.Key_Delete) {
                NodeManager.removeNode()
                NodeManager.removeEdge()
                event.accepted = true
            } else if (event.key == Qt.Key_Z && (event.modifiers & Qt.ControlModifier)) {
                NodeManager.undo()
                event.accepted = true
            } else if (event.key == Qt.Key_Y && (event.modifiers & Qt.ControlModifier)) {
                NodeManager.redo()
                event.accepted = true
            }
        }
    }
    // 侧边按钮栏
    Rectangle{
        id: left_side_bar
        width: Screen.width * 0.02
        height: parent.height
        border.width: 1  
        anchors.top: parent.top
        anchors.left: parent.left
        border.color: "lightgray" 
        Column {
            y:5
            spacing: 18
            anchors.horizontalCenter: parent.horizontalCenter
            IconButton {
                tip_info: "+"
                width:left_side_bar.width*0.9
                height:left_side_bar.width*0.9
                img_src: "qrc:/setting.png";
                onClickedLeft:(event) => {
                    // 使用 Qt.createComponent 动态加载 ImageLoadNode.qml
                    var component = Qt.createComponent("ImageLoadNode.qml");
                    if (component.status === Component.Ready) {
                        var imageNode = component.createObject(canvas);
                        if (imageNode === null) {
                            console.log("Failed to create object");
                        }else{
                            NodeManager.createNode(imageNode)
                        }
                    } else if (component.status === Component.Error) {
                        console.log("Error loading component:", component.errorString());
                    }
                }
            }
            IconButton {
                tip_info: "+"
                width:left_side_bar.width*0.9
                height:left_side_bar.width*0.9
                img_src: "qrc:/setting.png";
                onClickedLeft:(event) => {
                    // 使用 Qt.createComponent 动态加载 ImageInNode.qml
                    var component = Qt.createComponent("ImageShowNode.qml");
                    if (component.status === Component.Ready) {
                        var imageNode = component.createObject(canvas);
                        if (imageNode === null) {
                            console.log("Failed to create object");
                        }else{
                            NodeManager.createNode(imageNode)
                        }
                    } else if (component.status === Component.Error) {
                        console.log("Error loading component:", component.errorString());
                    }
                }
            }
        }
    }
}