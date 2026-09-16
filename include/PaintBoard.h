#pragma once
#include <memory>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QPainterPath>
#include "port/Port.hpp"
#include "utils/DAGraph.hpp"
#include "utils/Edge.hpp"


class PaintBoard : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
public:
    DAGraph m_graph;
    Edge m_drawing_edge;
    bool m_drawing_line = false;

    explicit PaintBoard(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {}

    void startDrawing(Port* start, QPointF pos) {
        if(!start) return;
        m_tmp_port = std::make_unique<Port>("__tmp__", PortType::Input,
                                            start->dataType(), pos, nullptr);
        m_drawing_edge = Edge(start, m_tmp_port.get());
        m_drawing_edge.midPoint = pos;
        m_drawing_line = true;
        update();
    }
    void moveDrawing(QPointF pos) {
        if(!m_drawing_line) return;
        if(m_drawing_edge.stop_port) m_drawing_edge.stop_port->setPosition(pos);
        m_drawing_edge.midPoint = pos;
        update();
    }
    void cancelDrawing() {
        m_drawing_line = false;
        m_drawing_edge = Edge{};
        m_tmp_port.reset();
        update();
    }
    void finishDrawing() { cancelDrawing(); }

    void paint(QPainter* painter) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        for(Edge& edge : m_graph.getAllEdges()){
            edge.calculateBezierPoint();
            edge.drawCurve(painter);
        }
        if(m_drawing_line){
            m_drawing_edge.drawCurve(painter);
        }
    }
private:
    std::unique_ptr<Port> m_tmp_port;
};
