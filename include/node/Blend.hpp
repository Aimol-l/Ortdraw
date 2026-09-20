#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 图像混合：两张图像按比例加权混合
class BlendNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int alpha READ alpha WRITE setAlpha NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Blend"; }
    QString category() const override { return "process"; }

    // A 的权重百分比，B 的权重为 100-alpha
    int alpha() const { return m_alpha; }
    void setAlpha(int v) { v = qBound(0, v, 100); if(m_alpha == v) return; m_alpha = v; emit paramsChanged(); }

    QVariantMap params() const override { return { { "alpha", alpha() } }; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("alpha")) setAlpha(p.value("alpha").toInt());
    }

    BlendNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "图像混合";

        m_description = "按比例混合两张图像（要求尺寸一致；低通道自动对齐到高通道）。";
        m_input_ports.push_back(new Port("图像A", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_input_ports.push_back(new Port("图像B", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~BlendNode(){}

signals:
    void paramsChanged();
private:
    int m_alpha = 50;
};
