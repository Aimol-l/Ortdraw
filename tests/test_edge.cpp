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
};

QTEST_MAIN(TestEdge)
#include "test_edge.moc"
