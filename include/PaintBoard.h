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
#include "Theme.h"


class PaintBoard : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY viewChanged)
    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY viewChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY viewChanged)
public:
    DAGraph m_graph;
    Edge m_drawing_edge;
    bool m_drawing_line = false;

    explicit PaintBoard(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
        connect(Theme::theme(), &Theme::changed, this, [this]{ update(); });
    }

    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z) { if(m_zoom == z) return; m_zoom = z; emit viewChanged(); update(); }
    qreal panX() const { return m_pan_x; }
    void setPanX(qreal x) { if(m_pan_x == x) return; m_pan_x = x; emit viewChanged(); update(); }
    qreal panY() const { return m_pan_y; }
    void setPanY(qreal y) { if(m_pan_y == y) return; m_pan_y = y; emit viewChanged(); update(); }

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
        Theme* theme = Theme::theme();
        painter->setRenderHint(QPainter::Antialiasing, true);
        // 画板与视口同尺寸，按视图变换在屏幕空间绘制世界坐标，负坐标也正常
        painter->save();
        painter->translate(m_pan_x, m_pan_y);
        painter->scale(m_zoom, m_zoom);
        for(Edge& edge : m_graph.getAllEdges()){
            edge.calculateBezierPoint();
            edge.drawCurve(painter, theme->wire(), theme->blue());
        }
        if(m_drawing_line){
            m_drawing_edge.drawCurve(painter, theme->wire(), theme->blue());
        }
        painter->restore();
    }
signals:
    void viewChanged();
private:
    std::unique_ptr<Port> m_tmp_port;
    qreal m_zoom = 1.0;
    qreal m_pan_x = 0.0;
    qreal m_pan_y = 0.0;
};
