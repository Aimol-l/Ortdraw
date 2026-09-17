#include <QtTest>
#include "utils/Edge.hpp"
#include "port/Port.hpp"

class TestEdge : public QObject {
    Q_OBJECT
private slots:
    void distanceOnSegment() {
        QCOMPARE(Edge::distanceToSegment(QPointF(5, 0), QPointF(0, 0), QPointF(10, 0)), 0.0);
    }
    void distancePerpendicular() {
        QCOMPARE(Edge::distanceToSegment(QPointF(5, 5), QPointF(0, 0), QPointF(10, 0)), 5.0);
    }
    void distanceBeyondEndpoint() {
        QCOMPARE(Edge::distanceToSegment(QPointF(15, 0), QPointF(0, 0), QPointF(10, 0)), 5.0);
    }
    void distanceDegenerateSegment() {
        QCOMPARE(Edge::distanceToSegment(QPointF(3, 4), QPointF(0, 0), QPointF(0, 0)), 5.0);
    }
    void hitRejectsNullPorts() {
        Edge e;
        QVERIFY(!e.isPointOnCurve(QPointF(0, 0)));
    }
    void hitEndpointsAndMid() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 0), nullptr);
        Edge e(&a, &b);
        QVERIFY(e.isPointOnCurve(QPointF(0, 0)));
        QVERIFY(e.isPointOnCurve(QPointF(100, 0)));
        QVERIFY(e.isPointOnCurve(QPointF(50, 0)));
    }
    void hitRejectsFarPoint() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 0), nullptr);
        Edge e(&a, &b);
        QVERIFY(!e.isPointOnCurve(QPointF(50, 20)));
    }
    void modeFromMapsKnownValues() {
        QVERIFY(Edge::modeFrom("spline") == LinkRenderMode::Spline);
        QVERIFY(Edge::modeFrom("linear") == LinkRenderMode::Linear);
        QVERIFY(Edge::modeFrom("straight") == LinkRenderMode::Straight);
    }
    void modeFromInvalidFallsBackToSpline() {
        QVERIFY(Edge::modeFrom("bogus") == LinkRenderMode::Spline);
        QVERIFY(Edge::modeFrom("") == LinkRenderMode::Spline);
    }
    void hitLinearEndpointsAndSegments() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 50), nullptr);
        Edge e(&a, &b);
        QVERIFY(e.isPointOnCurve(QPointF(0, 0), LinkRenderMode::Linear));
        QVERIFY(e.isPointOnCurve(QPointF(100, 50), LinkRenderMode::Linear));
        QVERIFY(e.isPointOnCurve(QPointF(25, 0), LinkRenderMode::Linear));
        QVERIFY(e.isPointOnCurve(QPointF(50, 25), LinkRenderMode::Linear));
    }
    void hitLinearRejectsFarPoint() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 50), nullptr);
        Edge e(&a, &b);
        QVERIFY(!e.isPointOnCurve(QPointF(0, 25), LinkRenderMode::Linear));
    }
    void hitLinearHandlesVertical() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(0, 100), nullptr);
        Edge e(&a, &b);
        QVERIFY(e.isPointOnCurve(QPointF(0, 0), LinkRenderMode::Linear));
        QVERIFY(e.isPointOnCurve(QPointF(0, 100), LinkRenderMode::Linear));
        QVERIFY(e.isPointOnCurve(QPointF(0, 50), LinkRenderMode::Linear));
        QVERIFY(!e.isPointOnCurve(QPointF(30, 50), LinkRenderMode::Linear));
    }
    void hitStraightEndpointsAndMid() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 50), nullptr);
        Edge e(&a, &b);
        QVERIFY(e.isPointOnCurve(QPointF(0, 0), LinkRenderMode::Straight));
        QVERIFY(e.isPointOnCurve(QPointF(100, 50), LinkRenderMode::Straight));
        QVERIFY(e.isPointOnCurve(QPointF(50, 25), LinkRenderMode::Straight));
    }
    void hitStraightRejectsFarPoint() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 50), nullptr);
        Edge e(&a, &b);
        QVERIFY(!e.isPointOnCurve(QPointF(0, 25), LinkRenderMode::Straight));
    }
    void hitSplineStillWorks() {
        Port a("a", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        Port b("b", PortType::Input, DataType::Image, QPointF(100, 0), nullptr);
        Edge e(&a, &b);
        QVERIFY(e.isPointOnCurve(QPointF(0, 0), LinkRenderMode::Spline));
        QVERIFY(e.isPointOnCurve(QPointF(100, 0), LinkRenderMode::Spline));
        QVERIFY(e.isPointOnCurve(QPointF(50, 0), LinkRenderMode::Spline));
        QVERIFY(!e.isPointOnCurve(QPointF(50, 20), LinkRenderMode::Spline));
    }
};

QTEST_MAIN(TestEdge)
#include "test_edge.moc"
