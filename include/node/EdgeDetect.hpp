#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 边缘检测：Sobel / Scharr / Laplacian / Canny，可选核大小与 Canny 双阈值
class EdgeDetectNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int method READ method WRITE setMethod NOTIFY paramsChanged)
    Q_PROPERTY(int kernel READ kernel WRITE setKernel NOTIFY paramsChanged)
    Q_PROPERTY(int low READ low WRITE setLow NOTIFY paramsChanged)
    Q_PROPERTY(int high READ high WRITE setHigh NOTIFY paramsChanged)
public:
    QString typeName() const override { return "EdgeDetect"; }
    QString category() const override { return "process"; }

    // 0=Sobel, 1=Scharr, 2=Laplacian, 3=Canny
    int method() const { return m_method; }
    void setMethod(int v) { v = qBound(0, v, 3); if(m_method == v) return; m_method = v; emit paramsChanged(); }

    int kernel() const { return m_kernel; }
    void setKernel(int v) {
        v = qBound(3, v, 7);
        if(v % 2 == 0) ++v;           // 核大小取奇数
        if(m_kernel == v) return;
        m_kernel = v; emit paramsChanged();
    }

    int low() const { return m_low; }
    void setLow(int v) { v = qBound(0, v, 255); if(m_low == v) return; m_low = v; emit paramsChanged(); }

    int high() const { return m_high; }
    void setHigh(int v) { v = qBound(0, v, 255); if(m_high == v) return; m_high = v; emit paramsChanged(); }

    QVariantMap params() const override {
        return { { "method", method() }, { "kernel", kernel() },
                 { "low", low() }, { "high", high() } };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("method")) setMethod(p.value("method").toInt());
        if(p.contains("kernel")) setKernel(p.value("kernel").toInt());
        if(p.contains("low"))    setLow(p.value("low").toInt());
        if(p.contains("high"))   setHigh(p.value("high").toInt());
    }

    EdgeDetectNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "边缘检测";

        m_description = "边缘检测：Sobel / Scharr / Laplacian / Canny。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~EdgeDetectNode(){}

signals:
    void paramsChanged();
private:
    int m_method = 3;
    int m_kernel = 3;
    int m_low = 100;
    int m_high = 200;
};
