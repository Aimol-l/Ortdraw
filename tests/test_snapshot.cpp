#include <QtTest>
#include "utils/Snapshot.hpp"
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "port/Port.hpp"

static Port* addPort(BaseNode* n, PortType t, DataType d){
    auto* p = new Port("p", t, d, QPointF(0,0), n);
    (t == PortType::Input ? n->getInPorts() : n->getOutPorts()).append(p);
    return p;
}

class TestSnapshot : public QObject {
    Q_OBJECT
private slots:
    void nodeSnapshotFields() {
        DAGraph g; auto* n = new BaseNode(); g.addNode(n);
        n->setX(10); n->setY(20); n->setWidth(220); n->setHeight(300);
        auto list = nodeSnapshotsOf(g);
        QCOMPARE(list.size(), 1);
        auto m = list.first().toMap();
        QVERIFY(m.contains("x")); QVERIFY(m.contains("y"));
        QVERIFY(m.contains("w")); QVERIFY(m.contains("h"));
        QVERIFY(m.contains("category")); QVERIFY(m.contains("selected"));
        QCOMPARE(m["x"].toReal(), 10.0);
        delete n;
    }
    void edgeSnapshotUsesPortPositions() {
        DAGraph g;
        auto* a = new BaseNode(); auto* b = new BaseNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in  = addPort(b, PortType::Input,  DataType::Image);
        out->setPosition(QPointF(100, 50));
        in->setPosition(QPointF(300, 80));
        QVERIFY(g.addEdge(out, in));
        auto list = edgeSnapshotsOf(g);
        QCOMPARE(list.size(), 1);
        auto m = list.first().toMap();
        QCOMPARE(m["fromX"].toReal(), 100.0);
        QCOMPARE(m["fromY"].toReal(), 50.0);
        QCOMPARE(m["toX"].toReal(), 300.0);
        QCOMPARE(m["toY"].toReal(), 80.0);
        delete a; delete b;
    }
    void emptyGraphGivesEmptyLists() {
        DAGraph g;
        QVERIFY(nodeSnapshotsOf(g).isEmpty());
        QVERIFY(edgeSnapshotsOf(g).isEmpty());
    }
};

QTEST_MAIN(TestSnapshot)
#include "test_snapshot.moc"
