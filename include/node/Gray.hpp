#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 灰度化：输入图像，输出单通道灰度图，无参数
class GrayNode : public BaseNode {
    Q_OBJECT

public:
    QString typeName() const override { return "Gray"; }
    QString category() const override { return "process"; }

    GrayNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "灰度化";

        m_description = "转换为单通道灰度图。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~GrayNode(){}
};
