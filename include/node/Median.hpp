#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 中值滤波：输入图像，节点内设定核大小（奇数）
class MedianNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int kernel READ kernel WRITE setKernel NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Median"; }
    QString category() const override { return "process"; }

    int kernel() const { return m_kernel; }
    void setKernel(int v) {
        v = qBound(3, v, 15);
        if(v % 2 == 0) ++v;            // 核大小取奇数
        v = qBound(3, v, 15);
        if(m_kernel == v) return;
        m_kernel = v; emit paramsChanged();
    }

    QVariantMap params() const override { return { { "kernel", kernel() } }; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("kernel")) setKernel(p.value("kernel").toInt());
    }

    MedianNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "中值滤波";

        m_description = "中值滤波降噪（核大小可调）。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~MedianNode(){}

signals:
    void paramsChanged();
private:
    int m_kernel = 5;
};
