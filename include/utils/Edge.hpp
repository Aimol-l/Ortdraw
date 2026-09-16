#pragma once

#include <QPointF>
#include <QColor>
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <limits>
#include "port/Port.hpp"

// 销毁的时候不要删除 port,所有权不在这里
struct Edge {
    bool seleected = false;
    QPointF midPoint;
    Port* start_port = nullptr;
    Port* stop_port = nullptr;

    Edge()=default;
    Edge(Port* start, Port* stop):seleected(false){
        this->start_port = start;
        this->stop_port = stop;
        this->calculateBezierPoint();
    }

    static qreal distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b){
        QPointF ab = b - a;
        qreal len2 = ab.x()*ab.x() + ab.y()*ab.y();
        if(len2 <= 1e-9) return QLineF(p, a).length();
        qreal t = ((p.x()-a.x())*ab.x() + (p.y()-a.y())*ab.y()) / len2;
        t = std::clamp(t, 0.0, 1.0);
        QPointF proj = a + t * ab;
        return QLineF(p, proj).length();
    }

    // 与 ComfyUI / LiteGraph 的 Spline 一致：
    // 控制点水平偏移量 = 两端欧氏距离 * 0.25
    static void controlPoints(const QPointF& p0, const QPointF& p3, QPointF& c1, QPointF& c2){
        const qreal dx = p3.x() - p0.x();
        const qreal dy = p3.y() - p0.y();
        const qreal dist = std::sqrt(dx*dx + dy*dy) * 0.25;
        c1 = QPointF(p0.x() + dist, p0.y());
        c2 = QPointF(p3.x() - dist, p3.y());
    }

    static QPointF bezierPoint(const QPointF& p0, const QPointF& c1, const QPointF& c2,
                               const QPointF& p3, qreal t){
        const qreal u = 1 - t;
        return u*u*u*p0 + 3*u*u*t*c1 + 3*u*t*t*c2 + t*t*t*p3;
    }

    bool isPointOnCurve(const QPointF& point) const {
        if(!start_port || !stop_port) return false;
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        QPointF P1, P2;
        controlPoints(P0, P3, P1, P2);
        const int samples = 24;
        qreal best = std::numeric_limits<qreal>::max();
        QPointF prev = P0;
        for(int i = 1; i <= samples; ++i){
            const qreal t = static_cast<qreal>(i) / samples;
            const QPointF cur = bezierPoint(P0, P1, P2, P3, t);
            best = std::min(best, distanceToSegment(point, prev, cur));
            prev = cur;
        }
        return best < 8.0;
    }

    void drawCurve(QPainter* painter, const QColor& wireColor, const QColor& selColor) const {
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        QPointF P1, P2;
        controlPoints(P0, P3, P1, P2);

        QPainterPath path;
        QPen pen(seleected ? selColor : wireColor, 2, Qt::SolidLine, Qt::RoundCap);
        painter->setPen(pen);
        path.moveTo(P0);
        path.cubicTo(P1, P2, P3);
        painter->drawPath(path);

        // 中点仅在选中时显示（ComfyUI 平时不画，悬停/选中才提示）
        if(seleected){
            painter->save();
            painter->setBrush(selColor);
            painter->setPen(Qt::NoPen);
            painter->drawEllipse(midPoint, 4, 4);
            painter->restore();
        }
    }

    // 计算贝塞尔曲线的中点
    void calculateBezierPoint() {
        if(!start_port || !stop_port) return;
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        QPointF P1, P2;
        controlPoints(P0, P3, P1, P2);
        this->midPoint = bezierPoint(P0, P1, P2, P3, 0.5);
    }

    // 定义相等运算符
    bool operator==(const Edge& other) const {
        return (start_port == other.start_port) && (stop_port == other.stop_port);
    }
};
