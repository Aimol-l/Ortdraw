#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 缩放：输入图像，节点内设定输出尺寸（按尺寸或按百分比）
class ResizeNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY paramsChanged)          // 0=按尺寸, 1=按百分比
    Q_PROPERTY(int outWidth READ outWidth WRITE setOutWidth NOTIFY paramsChanged)
    Q_PROPERTY(int outHeight READ outHeight WRITE setOutHeight NOTIFY paramsChanged)
    Q_PROPERTY(int percent READ percent WRITE setPercent NOTIFY paramsChanged)
    Q_PROPERTY(QString inputSizeText READ inputSizeText NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Resize"; }
    QString category() const override { return "process"; }

    int mode() const { return m_mode; }
    void setMode(int v) { if(m_mode == v) return; m_mode = v; emit paramsChanged(); }
    int outWidth() const { return m_out_width; }
    void setOutWidth(int v) { v = qBound(1, v, 8192); if(m_out_width == v) return; m_out_width = v; emit paramsChanged(); }
    int outHeight() const { return m_out_height; }
    void setOutHeight(int v) { v = qBound(1, v, 8192); if(m_out_height == v) return; m_out_height = v; emit paramsChanged(); }
    int percent() const { return m_percent; }
    void setPercent(int v) { v = qBound(1, v, 1000); if(m_percent == v) return; m_percent = v; emit paramsChanged(); }
    // 尚无执行引擎，输入尺寸为占位；接入执行引擎后由输入图像更新
    QString inputSizeText() const { return m_input_size_text; }
    void setInputSizeText(const QString& t) { if(m_input_size_text == t) return; m_input_size_text = t; emit paramsChanged(); }

    QVariantMap params() const override {
        return {
            { "mode", mode() },
            { "outWidth", outWidth() },
            { "outHeight", outHeight() },
            { "percent", percent() }
        };
    }
    void setParams(const QVariantMap& p) override {
        if(p.contains("mode"))      setMode(p.value("mode").toInt());
        if(p.contains("outWidth"))  setOutWidth(p.value("outWidth").toInt());
        if(p.contains("outHeight")) setOutHeight(p.value("outHeight").toInt());
        if(p.contains("percent"))   setPercent(p.value("percent").toInt());
    }

    ResizeNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "缩放";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ResizeNode(){}

signals:
    void paramsChanged();
private:
    int m_mode = 0;
    int m_out_width = 224;
    int m_out_height = 224;
    int m_percent = 100;
    QString m_input_size_text = "-- × --";
};
