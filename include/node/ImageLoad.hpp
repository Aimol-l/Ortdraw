
#pragma once

#include <QQuickItem>
#include "node/BaseNode.hpp"


// 将图片读取到这个节点中，这个节点只向后输出image
class ImageLoadNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
public:
    QString typeName() const override { return "ImageLoad"; }
    QString category() const override { return "input"; }

    QString path() const { return m_path; }
    void setPath(const QString& p) {
        if(m_path == p) return;
        m_path = p;
        emit pathChanged();
    }

    QVariantMap params() const override { return {{ "path", m_path }}; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("path")) setPath(p.value("path").toString());
    }

    ImageLoadNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "加载图片";
        m_output_ports.push_back(new Port("输出", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ImageLoadNode(){

    }

signals:
    void pathChanged();
private:
    QString m_path;
};
