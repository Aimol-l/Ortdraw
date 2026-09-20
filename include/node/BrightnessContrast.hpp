#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 亮度/对比度：亮度偏移与对比度百分比调整
class BrightnessContrastNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int brightness READ brightness WRITE setBrightness NOTIFY paramsChanged)
    Q_PROPERTY(int contrast READ contrast WRITE setContrast NOTIFY paramsChanged)
public:
    QString typeName() const override { return "BrightnessContrast"; }
    QString category() const override { return "process"; }

    int brightness() const { return m_brightness; }
    void setBrightness(int v) { v = qBound(-100, v, 100); if(m_brightness == v) return; m_brightness = v; emit paramsChanged(); }

    int contrast() const { return m_contrast; }
    void setContrast(int v) { v = qBound(0, v, 300); if(m_contrast == v) return; m_contrast = v; emit paramsChanged(); }

    QVariantMap params() const override {
        return { { "brightness", brightness() }, { "contrast", contrast() } };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("brightness")) setBrightness(p.value("brightness").toInt());
        if(p.contains("contrast"))   setContrast(p.value("contrast").toInt());
    }

    BrightnessContrastNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "亮度/对比度";

        m_description = "调整图像亮度与对比度。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~BrightnessContrastNode(){}

signals:
    void paramsChanged();
private:
    int m_brightness = 0;
    int m_contrast = 100;
};
