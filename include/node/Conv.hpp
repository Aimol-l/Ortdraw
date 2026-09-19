#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 卷积：输入图像 + 卷积核，输出卷积结果
class ConvNode : public BaseNode {
    Q_OBJECT
public:
    QString typeName() const override { return "Conv"; }
    QString category() const override { return "math"; }

    ConvNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "卷积";

        m_description = "用卷积核对图像做卷积（核由张量节点提供）。";
        m_input_ports.push_back(new Port("图像",   PortType::Input, DataType::Image,  QPointF(0,0), this));
        m_input_ports.push_back(new Port("卷积核", PortType::Input, DataType::Tensor, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ConvNode(){}
};
