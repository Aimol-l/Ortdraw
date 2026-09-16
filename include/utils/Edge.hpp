#pragma once

#include <QPointF>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <algorithm>
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
    bool isPointOnCurve(const QPointF& point) const {
        if(!start_port || !stop_port) return false;
        const QPointF P0 = start_port->position();
        const QPointF P1 = P0 + QPointF{200, 0};
        const QPointF P2 = stop_port->position() - QPointF{200, 0};
        const QPointF P3 = stop_port->position();
        const int samples = 24;
        qreal best = std::numeric_limits<qreal>::max();
        QPointF prev = P0;
        for(int i = 1; i <= samples; ++i){
            qreal t = static_cast<qreal>(i) / samples;
            qreal u = 1 - t;
            QPointF cur = u*u*u*P0 + 3*u*u*t*P1 + 3*u*t*t*P2 + t*t*t*P3;
            best = std::min(best, distanceToSegment(point, prev, cur));
            prev = cur;
        }
        return best < 8.0;
    }
    void drawCurve(QPainter* painter){
        QPainterPath path;
        QPen pen(seleected ? Qt::red : Qt::green,2); // 根据选中状态设置颜色
        painter->setPen(pen);  // 设置画笔颜色和宽度
        path.moveTo(start_port->position()); // 移动到起点
        // 计算控制点，确保它们在水平线上
        QPointF controlPoint1 = start_port->position() + QPointF{200, 0};
        QPointF controlPoint2 = stop_port->position()  - QPointF{200, 0};
        path.cubicTo(controlPoint1, controlPoint2, stop_port->position()); // 绘制三次贝塞尔曲线
        painter->drawPath(path); // 绘制路径
        painter->save();
        // 绘制中间点
        painter->setBrush(seleected ? Qt::red : Qt::green);
        painter->drawEllipse(midPoint, 4, 4); // 绘制一个半径为5的点
        painter->restore();
    }
    // 计算贝塞尔曲线的中点
    void calculateBezierPoint() {
        auto P0 = start_port->position();
        auto P1 = start_port->position() + QPointF{200, 0};
        auto P2 = stop_port->position()  - QPointF{200, 0};
        auto P3 = stop_port->position();
        double t = 0.5;
        double u = 1 - t;
        double tt = t * t;
        double uu = u * u;
        double uuu = uu * u;
        double ttt = tt * t;
        QPointF point = uuu * P0; // (1-t)^3 * P0
        point += 3 * uu * t * P1; // 3 * (1-t)^2 * t * P1
        point += 3 * u * tt * P2; // 3 * (1-t) * t^2 * P2
        point += ttt * P3; // t^3 * P3
        this->midPoint = point;
    }
    // 定义相等运算符
    bool operator==(const Edge& other) const {
        return (start_port == other.start_port) && (stop_port == other.stop_port);
    }
};
