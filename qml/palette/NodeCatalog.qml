pragma Singleton
import QtQuick

QtObject {
    readonly property var items: [
        { type:"ImageLoad", title:"加载图片", desc:"从磁盘读取图像",     cat:"input",   icon:"image"   },
        { type:"ImageShow", title:"图片显示", desc:"预览处理结果",       cat:"output",  icon:"monitor" },
        { type:"Resize",    title:"缩放",     desc:"双线性插值调整尺寸", cat:"process", icon:"resize"  },
        { type:"Blur",      title:"高斯模糊", desc:"可调核大小的模糊",   cat:"process", icon:"blur"    },
        { type:"Threshold", title:"阈值二值化", desc:"固定 / 自适应阈值", cat:"process", icon:"threshold" },
        { type:"Gray",      title:"灰度化",   desc:"转换为单通道灰度图", cat:"process", icon:"gray"    },
        { type:"Conv",      title:"卷积",     desc:"自定义卷积核",       cat:"math",    icon:"conv"    },
        { type:"Tensor",    title:"张量 / 卷积核", desc:"自定义卷积核", cat:"math",    icon:"conv"    }
    ]

    readonly property var categoryNames: ({
        input:"输入 / 输出", process:"图像处理", math:"数学 / 张量", output:"输出 / 显示"
    })

    function componentUrl(type) {
        if (type === "ImageLoad") return "qrc:/ImageLoadNode.qml"
        if (type === "ImageShow") return "qrc:/ImageShowNode.qml"
        if (type === "Resize")    return "qrc:/ResizeNode.qml"
        if (type === "Blur")      return "qrc:/BlurNode.qml"
        if (type === "Threshold") return "qrc:/ThresholdNode.qml"
        if (type === "Gray")      return "qrc:/GrayNode.qml"
        if (type === "Conv")      return "qrc:/ConvNode.qml"
        if (type === "Tensor")    return "qrc:/TensorNode.qml"
        return ""
    }

    function byType(type) {
        return items.find(function(i) { return i.type === type })
    }
}
