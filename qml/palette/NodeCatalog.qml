pragma Singleton
import QtQuick

QtObject {
    readonly property var items: [
        { type:"ImageLoad", title:"加载图片", desc:"从磁盘读取图像",     cat:"input",   icon:"image"   },
        { type:"ImageSave", title:"保存图片", desc:"将图像写入文件",     cat:"output",  icon:"save"    },
        { type:"ImageShow", title:"图片显示", desc:"预览处理结果",       cat:"output",  icon:"monitor" },
        { type:"Resize",    title:"缩放",     desc:"双线性插值调整尺寸", cat:"process", icon:"resize"  },
        { type:"Blur",      title:"高斯模糊", desc:"可调核大小的模糊",   cat:"process", icon:"blur"    },
        { type:"Median",    title:"中值滤波", desc:"中值滤波降噪",       cat:"process", icon:"median"  },
        { type:"Morphology", title:"形态学",  desc:"膨胀/腐蚀/开/闭/梯度", cat:"process", icon:"morph"  },
        { type:"Blend",     title:"图像混合", desc:"按比例混合两张图像", cat:"process", icon:"blend"   },
        { type:"Threshold", title:"阈值二值化", desc:"固定 / 自适应阈值", cat:"process", icon:"threshold" },
        { type:"Gray",      title:"灰度化",   desc:"转换为单通道灰度图", cat:"process", icon:"gray"    },
        { type:"EdgeDetect", title:"边缘检测", desc:"Sobel / Scharr / Laplacian / Canny", cat:"process", icon:"edge" },
        { type:"Crop",      title:"裁剪",     desc:"按矩形区域裁剪图像", cat:"process", icon:"crop"   },
        { type:"FlipRotate", title:"翻转/旋转", desc:"翻转或旋转图像",      cat:"process", icon:"rotate" },
        { type:"BrightnessContrast", title:"亮度/对比度", desc:"调整亮度与对比度", cat:"process", icon:"adjust" },
        { type:"Conv",      title:"卷积",     desc:"自定义卷积核",       cat:"math",    icon:"conv"    },
        { type:"Tensor",    title:"张量 / 卷积核", desc:"自定义卷积核", cat:"math",    icon:"conv"    }
    ]

    readonly property var categoryNames: ({
        input:"输入 / 输出", process:"图像处理", math:"数学 / 张量", output:"输出 / 显示"
    })

    function componentUrl(type) {
        if (type === "ImageLoad") return "qrc:/ImageLoadNode.qml"
        if (type === "ImageSave") return "qrc:/ImageSaveNode.qml"
        if (type === "ImageShow") return "qrc:/ImageShowNode.qml"
        if (type === "Resize")    return "qrc:/ResizeNode.qml"
        if (type === "Blur")      return "qrc:/BlurNode.qml"
        if (type === "Median")    return "qrc:/MedianNode.qml"
        if (type === "Morphology") return "qrc:/MorphologyNode.qml"
        if (type === "Blend")     return "qrc:/BlendNode.qml"
        if (type === "Threshold") return "qrc:/ThresholdNode.qml"
        if (type === "Gray")      return "qrc:/GrayNode.qml"
        if (type === "EdgeDetect") return "qrc:/EdgeDetectNode.qml"
        if (type === "Crop")      return "qrc:/CropNode.qml"
        if (type === "FlipRotate") return "qrc:/FlipRotateNode.qml"
        if (type === "BrightnessContrast") return "qrc:/BrightnessContrastNode.qml"
        if (type === "Conv")      return "qrc:/ConvNode.qml"
        if (type === "Tensor")    return "qrc:/TensorNode.qml"
        return ""
    }

    function byType(type) {
        return items.find(function(i) { return i.type === type })
    }
}
