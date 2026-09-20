#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 裁剪：按矩形区域裁剪图像，w/h <= 0 表示取到图像边界
class CropNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int x READ x WRITE setX NOTIFY paramsChanged)
    Q_PROPERTY(int y READ y WRITE setY NOTIFY paramsChanged)
    Q_PROPERTY(int w READ w WRITE setW NOTIFY paramsChanged)
    Q_PROPERTY(int h READ h WRITE setH NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Crop"; }
    QString category() const override { return "process"; }

    int x() const { return m_x; }
    void setX(int v) { if(v < 0) v = 0; if(m_x == v) return; m_x = v; emit paramsChanged(); }
    int y() const { return m_y; }
    void setY(int v) { if(v < 0) v = 0; if(m_y == v) return; m_y = v; emit paramsChanged(); }
    int w() const { return m_w; }
    void setW(int v) { if(m_w == v) return; m_w = v; emit paramsChanged(); }
    int h() const { return m_h; }
    void setH(int v) { if(m_h == v) return; m_h = v; emit paramsChanged(); }

    QVariantMap params() const override {
        return { { "x", x() }, { "y", y() }, { "w", w() }, { "h", h() } };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("x")) setX(p.value("x").toInt());
        if(p.contains("y")) setY(p.value("y").toInt());
        if(p.contains("w")) setW(p.value("w").toInt());
        if(p.contains("h")) setH(p.value("h").toInt());
    }

    CropNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "裁剪";

        m_description = "按矩形区域裁剪图像。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~CropNode(){}

signals:
    void paramsChanged();
private:
    int m_x = 0;
    int m_y = 0;
    int m_w = 0;
    int m_h = 0;
};
