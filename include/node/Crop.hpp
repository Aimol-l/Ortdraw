#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 裁剪：x/y 为左上角(>=0)，w/h 为子图大小(>0；0 取到边界)，越界时报错
class CropNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int cropX READ cropX WRITE setCropX NOTIFY paramsChanged)
    Q_PROPERTY(int cropY READ cropY WRITE setCropY NOTIFY paramsChanged)
    Q_PROPERTY(int cropW READ cropW WRITE setCropW NOTIFY paramsChanged)
    Q_PROPERTY(int cropH READ cropH WRITE setCropH NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Crop"; }
    QString category() const override { return "process"; }

    int cropX() const { return m_x; }
    void setCropX(int v) { if(v < 0) v = 0; if(m_x == v) return; m_x = v; emit paramsChanged(); }
    int cropY() const { return m_y; }
    void setCropY(int v) { if(v < 0) v = 0; if(m_y == v) return; m_y = v; emit paramsChanged(); }
    int cropW() const { return m_w; }
    void setCropW(int v) { if(m_w == v) return; m_w = v; emit paramsChanged(); }
    int cropH() const { return m_h; }
    void setCropH(int v) { if(m_h == v) return; m_h = v; emit paramsChanged(); }

    QVariantMap params() const override {
        return { { "x", cropX() }, { "y", cropY() }, { "w", cropW() }, { "h", cropH() } };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("x")) setCropX(p.value("x").toInt());
        if(p.contains("y")) setCropY(p.value("y").toInt());
        if(p.contains("w")) setCropW(p.value("w").toInt());
        if(p.contains("h")) setCropH(p.value("h").toInt());
    }

    CropNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "裁剪";

        m_description = "按矩形裁剪：x/y 为左上角(>=0)，w/h 为子图大小(>0；为 0 取到边界)；越界(x+w>W 或 y+h>H)时报错。";
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
