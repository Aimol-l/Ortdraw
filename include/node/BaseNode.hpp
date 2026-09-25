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
#include <QVariantMap>
#include <opencv2/opencv.hpp>

#include "port/Port.hpp"


struct PortSpec { QString name; DataType type; };


class BaseNode:public QQuickItem{
    Q_OBJECT  
    Q_PROPERTY(QUuid uuid READ uuid CONSTANT)
    Q_PROPERTY(QColor color READ color WRITE setColor)   // 背景颜色

    Q_PROPERTY(bool selected READ selected  WRITE setSelected NOTIFY selectedChanged)
    Q_PROPERTY(QString typeName READ typeName CONSTANT)
    Q_PROPERTY(QString category READ category CONSTANT)
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)     // 节点名称
    Q_PROPERTY(QQmlListProperty<Port> inputPorts READ inputPorts NOTIFY inputPortsChanged)
    Q_PROPERTY(QQmlListProperty<Port> outputPorts READ outputPorts  NOTIFY outputPortsChanged)
protected:
    QUuid m_id;
    QString m_name;
    QString m_description;
    QColor m_bg_color;  //背景颜色
    bool is_selected;
    // 被用户手动缩放过：保存/加载时恢复尺寸（否则交给 autoHeight 自适应）
    bool m_size_pinned = false;
    QPointF m_start_pos;
    qreal min_width = 220;
    qreal min_height = 120;
    QList<Port*> m_input_ports; 
    QList<Port*> m_output_ports; 
public:
    virtual QString typeName() const { return "Base"; }
    virtual QString category() const { return "process"; }
    Q_INVOKABLE qreal getMinWidth(){return min_width;}
    Q_INVOKABLE qreal getMinHeight(){return min_height;}
    // 读取图文件时恢复节点 uuid，使连线可按保存的 uuid 重新匹配
    Q_INVOKABLE void setUuid(const QString& id) { m_id = QUuid(id); }
    // 节点参数序列化接口：子类按需覆写，用于图文件的保存/读取
    Q_INVOKABLE virtual QVariantMap params() const { return {}; }
    Q_INVOKABLE virtual void setParams(const QVariantMap&) {}
    // 非端口“显示数据”通道（分类 Top-K 等）。默认忽略，需要展示的节点覆写即可；
    // 放在基类是为了让 run()/nodeDisplay 对所有节点 invokeMethod 都成立（否则会打印
    // “No such method ... setDisplayData”）。
    Q_INVOKABLE virtual void setDisplayData(const QVariantMap&) {}
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
        this->setFlag(QQuickItem::ItemIsFocusScope, true); // 允许管理子组件的焦点
        }
    ~BaseNode() {
        qDeleteAll(m_input_ports);
        m_input_ports.clear();
        qDeleteAll(m_output_ports);
        m_output_ports.clear();
    }
    // Getters
    bool selected()const {return is_selected;}
    bool sizePinned() const { return m_size_pinned; }
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
        if(this->is_selected == val) return;
        this->is_selected = val;
        this->update();
        emit selectedChanged();
    }
    void setName(const QString &name) {
        if(m_name == name) return;
        m_name = name;
        emit nameChanged();
    }
    void setDescription(const QString& d) {
        if(m_description == d) return;
        m_description = d;
        emit descriptionChanged();
    }
    void setColor(const QColor &bgColor) {
        m_bg_color = bgColor;
    }
    void setSizePinned(bool v) { m_size_pinned = v; }
    // 重建输入/输出端口（动态节点用）；调用方需先断开将失效端口的连线
    void rebuildPorts(const QVector<PortSpec>& ins, const QVector<PortSpec>& outs) {
        qDeleteAll(m_input_ports);  m_input_ports.clear();
        qDeleteAll(m_output_ports); m_output_ports.clear();
        for (const auto& s : ins)
            m_input_ports.push_back(new Port(s.name, PortType::Input, s.type, QPointF(0,0), this));
        for (const auto& s : outs)
            m_output_ports.push_back(new Port(s.name, PortType::Output, s.type, QPointF(0,0), this));
        emit inputPortsChanged();
        emit outputPortsChanged();
    }
signals:
    void nameChanged();
    void descriptionChanged();
    void selectedChanged();
    void inputPortsChanged();
    void outputPortsChanged();
};
