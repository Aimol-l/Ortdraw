#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 形态学：腐蚀 / 膨胀 / 开 / 闭 / 梯度
class MorphologyNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int op READ op WRITE setOp NOTIFY paramsChanged)
    Q_PROPERTY(int kernel READ kernel WRITE setKernel NOTIFY paramsChanged)
    Q_PROPERTY(int iterations READ iterations WRITE setIterations NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Morphology"; }
    QString category() const override { return "process"; }

    // 0=腐蚀, 1=膨胀, 2=开, 3=闭, 4=梯度
    int op() const { return m_op; }
    void setOp(int v) { v = qBound(0, v, 4); if(m_op == v) return; m_op = v; emit paramsChanged(); }

    int kernel() const { return m_kernel; }
    void setKernel(int v) {
        v = qBound(3, v, 15);
        if(v % 2 == 0) ++v;            // 核大小取奇数
        v = qBound(3, v, 15);
        if(m_kernel == v) return;
        m_kernel = v; emit paramsChanged();
    }

    int iterations() const { return m_iterations; }
    void setIterations(int v) { v = qBound(1, v, 5); if(m_iterations == v) return; m_iterations = v; emit paramsChanged(); }

    QVariantMap params() const override {
        return { { "op", op() }, { "kernel", kernel() }, { "iterations", iterations() } };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("op"))         setOp(p.value("op").toInt());
        if(p.contains("kernel"))     setKernel(p.value("kernel").toInt());
        if(p.contains("iterations")) setIterations(p.value("iterations").toInt());
    }

    MorphologyNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "形态学";

        m_description = "膨胀/腐蚀/开/闭/梯度。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~MorphologyNode(){}

signals:
    void paramsChanged();
private:
    int m_op = 0;
    int m_kernel = 3;
    int m_iterations = 1;
};
