#include <QtTest>
#include "port/Port.hpp"

class TestPort : public QObject {
    Q_OBJECT
private slots:
    void testMoveDelta() {
        Port p("p", PortType::Input, DataType::Image, QPointF(0, 0), nullptr);
        p.movedeltaPos(QPointF(10, -5));
        QCOMPARE(p.position(), QPointF(10, -5));
    }
    void testConnectedFlag() {
        Port p("p", PortType::Input, DataType::Image, QPointF(0, 0), nullptr);
        QVERIFY(!p.isConnected());
        p.setConnected(true);
        QVERIFY(p.isConnected());
    }
    void testDataType() {
        Port p("p", PortType::Output, DataType::Number, QPointF(1, 2), nullptr);
        QCOMPARE(p.dataType(), DataType::Number);
        QCOMPARE(p.type(), static_cast<int>(PortType::Output));
    }
};

QTEST_MAIN(TestPort)
#include "test_port.moc"
