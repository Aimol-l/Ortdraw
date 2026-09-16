#pragma once

#include <variant>
#include <vector>
#include <string>
#include <print>
#include <QDebug>
#include <QObject>
#include <iostream>
#include <QQuickItem>
#include <QUuid>
#include <QString>
#include <QColor>
#include <QSGGeometry>
#include <QSGGeometryNode>
#include <QSGFlatColorMaterial>
#include <QSGSimpleRectNode>
#include <opencv2/opencv.hpp>

#include "port/Port.hpp"




class BaseNode:public QQuickItem{
    Q_OBJECT  
    Q_PROPERTY(QUuid uuid READ uuid)
    Q_PROPERTY(QColor color READ color WRITE setColor)   // 背景颜色

    Q_PROPERTY(bool selected READ selected  WRITE setSelected)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)     // 节点名称
    Q_PROPERTY(QQmlListProperty<Port> inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(QQmlListProperty<Port> outputPorts READ outputPorts  NOTIFY outputPortsChanged)
protected:
    QUuid m_id;
    QString m_name;
    QString m_description;
    QColor m_bg_color;  //背景颜色
    bool is_selected;
    QPointF m_start_pos;
    qreal min_width = 220;
    qreal min_height = 300;
    QList<Port*> m_input_ports; 
    QList<Port*> m_output_ports; 
public:
    Q_INVOKABLE qreal getMinWidth(){return min_width;}
    Q_INVOKABLE qreal getMinHeight(){return min_height;}
    Q_INVOKABLE void setInputPortPosition(int index, qreal x, qreal y){
        if(index >= 0 && index < m_input_ports.size())
            m_input_ports[index]->setPosition(QPointF(x, y));
    }
    Q_INVOKABLE void setOutputPortPosition(int index, qreal x, qreal y){
        if(index >= 0 && index < m_output_ports.size())
            m_output_ports[index]->setPosition(QPointF(x, y));
    }
    // virtual NodeData execute(const NodeData& input) = 0;
    QList<Port*> getPorts(){
        QList<Port*> ports_id;
        for(auto port:m_input_ports){
            ports_id.append(port);
        }
        for(auto port:m_output_ports){
            ports_id.append(port);
        }
        return ports_id;
    }
    QList<Port*>& getOutPorts(){
        return m_output_ports;
    }
    QList<Port*>& getInPorts(){
        return m_input_ports;
    }
    explicit BaseNode(QQuickItem *parent = nullptr): QQuickItem(parent),
          m_id(QUuid::createUuid()),
          m_name("Unknow"),
          m_description("No Description"),
          m_bg_color(Qt::gray),
          is_selected(false)
    {
        this->setFlag(QQuickItem::ItemHasContents, true); // 启用自定义绘制
        this->setFlag(QQuickItem::ItemIsFocusScope, true); // 允许管理子组件的焦点
        this->setAcceptedMouseButtons(Qt::LeftButton); // 在构造函数中添加
        qDebug()<<this->m_id;
        }
    ~BaseNode() {
        qDeleteAll(m_input_ports);
        m_input_ports.clear();
        qDeleteAll(m_output_ports);
        m_output_ports.clear();
    }
    // Getters
    bool selected()const {return is_selected;}
    QString name() const { return m_name; }
    QUuid uuid()const{return m_id;}
    QString description() const { return m_description; }
    QColor color() const { return m_bg_color; }
    QQmlListProperty<Port> inputPorts() {
        return QQmlListProperty<Port>(this, &m_input_ports);
    }
    QQmlListProperty<Port> outputPorts() {
        return QQmlListProperty<Port>(this, &m_output_ports);
    }
    // Setters
    void setSelected(const bool val){
        this->is_selected = val;
        this->update();
    }
    void setName(const QString &name) {
        m_name = name;
    }
    void setDescription(const QString &description) {
        m_description = description;
    }
    void setColor(const QColor &bgColor) {
        m_bg_color = bgColor;
    }
    void mousePressEvent(QMouseEvent* event){
        std::println("mousePressEvent");
        // this->update();
    }
    void mouseMoveEvent(QMouseEvent* event){
        this->update();
        std::println("mouseMoveEvent");
    }
    void mouseReleaseEvent(QMouseEvent* event) {
        // this->update();
    }
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override {
        QSGSimpleRectNode* backgroundNode = static_cast<QSGSimpleRectNode*>(oldNode);
        if (!backgroundNode) 
            backgroundNode = new QSGSimpleRectNode();
        QRectF bounds = this->boundingRect();
        backgroundNode->setRect(bounds);
        if(this->is_selected){
            this->m_bg_color = QColor("#77b3fe");
        }else{
            this->m_bg_color = Qt::gray;
        }
        backgroundNode->setColor(m_bg_color);
        return backgroundNode;
    }

signals:
    void nameChanged();
    void inputPortsChanged();
    void outputPortsChanged();
};
