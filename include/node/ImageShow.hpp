#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

class ImageShowNode : public BaseNode {
    Q_OBJECT

public:
    QString typeName() const override { return "ImageShow"; }
    QString category() const override { return "output"; }

    ImageShowNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "图片显示";
        m_input_ports.push_back(new Port("输入",PortType::Input,DataType::Image,QPointF(0,0),this));
    }
    ~ImageShowNode(){
        
    }

    // 输入的数据类型应该是：QImage，cv::Mat,
    // NodeData execute(const NodeData& input) override {
    //     if (std::holds_alternative<int>(input)) {
    //         return std::get<int>(input) + 10;
    //     }
    //     return input; // 类型不匹配时，原样返回
    // }
};
