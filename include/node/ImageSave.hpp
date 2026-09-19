#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"

// 保存图片：输入图像，写入磁盘后透传输出图像（便于链到显示节点）
class ImageSaveNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY paramsChanged)
public:
    QString typeName() const override { return "ImageSave"; }
    QString category() const override { return "output"; }

    QString path() const { return m_path; }
    void setPath(const QString& p) {
        if(m_path == p) return;
        m_path = p; emit paramsChanged();
    }

    QVariantMap params() const override { return { { "path", m_path } }; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("path")) setPath(p.value("path").toString());
    }

    ImageSaveNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "保存图片";

        m_description = "将输入图像保存到文件。";
        m_input_ports.push_back(new Port("图像", PortType::Input, DataType::Image, QPointF(0,0), this));
        m_output_ports.push_back(new Port("图像", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ImageSaveNode(){}

signals:
    void paramsChanged();
private:
    QString m_path;
};
