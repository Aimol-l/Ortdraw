#pragma once

#include <QPointF>
#include <QColor>
#include <QPen>
#include <QPainter>
#include <QPainterPath>
#include <QLineF>
#include <QString>
#include <QVector>
#include <algorithm>
#include <cmath>
#include <limits>
#include "port/Port.hpp"

enum class LinkRenderMode { Spline, Linear, Straight };

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

    static LinkRenderMode modeFrom(const QString& s) {
        if(s == QStringLiteral("linear")) return LinkRenderMode::Linear;
        if(s == QStringLiteral("straight")) return LinkRenderMode::Straight;
        return LinkRenderMode::Spline;
    }

    // 正交折线顶点：p0 -> (p0.x+L, p0.y) -> (p3.x-L, p3.y) -> p3
    // dx == 0 时退化为直线，避免水平偏移反向交叉
    static QVector<QPointF> linearPoints(const QPointF& p0, const QPointF& p3) {
        const qreal dx = p3.x() - p0.x();
        if(std::abs(dx) < 1e-6) return {p0, p3};
        const qreal L = std::max<qreal>(15.0, std::abs(dx) * 0.25);
        return {p0, QPointF(p0.x() + L, p0.y()), QPointF(p3.x() - L, p3.y()), p3};
    }

    static QPainterPath roundedLinearPath(const QPointF& p0, const QPointF& p3) {
        const QVector<QPointF> pts = linearPoints(p0, p3);
        QPainterPath path;
        if(pts.size() < 3){
            path.moveTo(p0);
            path.lineTo(p3);
            return path;
        }
        const qreal r = 6.0;
        auto toward = [](const QPointF& from, const QPointF& to, qreal d){
            const QPointF v = to - from;
            const qreal len = std::hypot(v.x(), v.y());
            if(len < 1e-9) return to;
            return from + v * (d / len);
        };
        const QPointF& a = pts[1];
        const QPointF& b = pts[2];
        path.moveTo(pts[0]);
        path.lineTo(toward(a, pts[0], r));
        path.quadTo(a, toward(a, b, r));
        path.lineTo(toward(b, a, r));
        path.quadTo(b, toward(b, pts[3], r));
        path.lineTo(pts[3]);
        return path;
    }

    bool isPointOnCurve(const QPointF& point,
                        LinkRenderMode mode = LinkRenderMode::Spline) const {
        if(!start_port || !stop_port) return false;
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        qreal best = std::numeric_limits<qreal>::max();
        if(mode == LinkRenderMode::Straight){
            best = distanceToSegment(point, P0, P3);
        } else if(mode == LinkRenderMode::Linear){
            const QVector<QPointF> pts = linearPoints(P0, P3);
            for(int i = 0; i + 1 < pts.size(); ++i)
                best = std::min(best, distanceToSegment(point, pts[i], pts[i + 1]));
        } else {
            QPointF P1, P2;
            controlPoints(P0, P3, P1, P2);
            const int samples = 24;
            QPointF prev = P0;
            for(int i = 1; i <= samples; ++i){
                const qreal t = static_cast<qreal>(i) / samples;
                const QPointF cur = bezierPoint(P0, P1, P2, P3, t);
                best = std::min(best, distanceToSegment(point, prev, cur));
                prev = cur;
            }
        }
        return best < 8.0;
    }

    void drawCurve(QPainter* painter, const QColor& wireColor, const QColor& selColor,
                   LinkRenderMode mode, int width) const {
        if(!start_port || !stop_port) return;
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        QPen pen(seleected ? selColor : wireColor, width, Qt::SolidLine, Qt::RoundCap);
        painter->setPen(pen);

        if(mode == LinkRenderMode::Straight){
            painter->drawLine(P0, P3);
            return;
        }
        if(mode == LinkRenderMode::Linear){
            painter->drawPath(roundedLinearPath(P0, P3));
            return;
        }
        QPointF P1, P2;
        controlPoints(P0, P3, P1, P2);
        QPainterPath path;
        path.moveTo(P0);
        path.cubicTo(P1, P2, P3);
        painter->drawPath(path);
    }

    // 计算中点：Spline 用三次贝塞尔 t=0.5；Linear/Straight 用两端中点
    void calculateBezierPoint(LinkRenderMode mode = LinkRenderMode::Spline) {
        if(!start_port || !stop_port) return;
        const QPointF P0 = start_port->position();
        const QPointF P3 = stop_port->position();
        if(mode != LinkRenderMode::Spline){
            this->midPoint = (P0 + P3) * 0.5;
            return;
        }
        QPointF P1, P2;
        controlPoints(P0, P3, P1, P2);
        this->midPoint = bezierPoint(P0, P1, P2, P3, 0.5);
    }

    // 定义相等运算符
    bool operator==(const Edge& other) const {
        return (start_port == other.start_port) && (stop_port == other.stop_port);
    }
};
