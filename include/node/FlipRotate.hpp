#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 翻转/旋转：水平/垂直翻转，或 90/180/270 度旋转
class FlipRotateNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY paramsChanged)
public:
    QString typeName() const override { return "FlipRotate"; }
    QString category() const override { return "process"; }

    // 0=水平翻转, 1=垂直翻转, 2=旋转90°, 3=旋转180°, 4=旋转270°
    int mode() const { return m_mode; }
    void setMode(int v) { v = qBound(0, v, 4); if(m_mode == v) return; m_mode = v; emit paramsChanged(); }

    QVariantMap params() const override { return { { "mode", mode() } }; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("mode")) setMode(p.value("mode").toInt());
    }

    FlipRotateNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "翻转/旋转";

        m_description = "水平/垂直翻转，或 90/180/270 度旋转。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~FlipRotateNode(){}

signals:
    void paramsChanged();
private:
    int m_mode = 0;
};
