
#pragma once

#include <QQuickItem>
#include <QSGGeometryNode>
#include <QPainterPath>
#include <QSGFlatColorMaterial>
#include "node/BaseNode.hpp"
#include <print>


// 将图片读取到这个节点中，这个节点只向后输出image
class ImageLoadNode : public BaseNode {
    Q_OBJECT
public:

    ImageLoadNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "加载图片";
        m_output_ports.push_back(new Port("输出", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ImageLoadNode(){

    }

};
