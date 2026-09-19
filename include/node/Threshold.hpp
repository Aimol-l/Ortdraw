#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 阈值二值化：输入图像，节点内拖动条设定阈值(0-255)
class ThresholdNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int threshold READ threshold WRITE setThreshold NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Threshold"; }
    QString category() const override { return "process"; }

    int threshold() const { return m_threshold; }
    void setThreshold(int v) { v = qBound(0, v, 255); if(m_threshold == v) return; m_threshold = v; emit paramsChanged(); }

    QVariantMap params() const override { return { { "threshold", threshold() } }; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("threshold")) setThreshold(p.value("threshold").toInt());
    }

    ThresholdNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "阈值二值化";

        m_description = "固定阈值二值化，输出 0/255。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ThresholdNode(){}

signals:
    void paramsChanged();
private:
    int m_threshold = 128;
};
