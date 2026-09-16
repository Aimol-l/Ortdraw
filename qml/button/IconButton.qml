import QtQuick 2.0
import QtQuick.Controls 
Rectangle {
    id: rec
    radius: 5
    property alias img_src: icon.source
    property color clr_enter: "#dcdcdc"
    property color clr_exit: "#ffffff"
    property color clr_click: "#aba9b2"
    property color clr_release: "#ffffff"
    property string tip_info: "default"
    //自定义点击信号
    signal clickedLeft()
    signal clickedRight()
    signal release()
    color:"white"
    Image {
        id: icon
        clip: true
        anchors.fill:parent
        source: ""
        fillMode: Image.PreserveAspectFit
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.left: parent.left
    }
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        //接受左键和右键输入
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        onClicked: (event)=>{
            //左键点击
            if (event.button === Qt.LeftButton){
                parent.clickedLeft()
            }else if(event.button === Qt.RightButton){
                parent.clickedRight()
            }
        }
        ToolTip{
            text: tip_info
        }
        onPressed: {
            color = clr_click
        }
        //释放
        onReleased: {
            color = clr_enter
            parent.release()
        }
        //指针进入
        onEntered: {
            color = clr_enter
        }
        //指针退出
        onExited: {
            color = clr_exit
        }
    }
}

