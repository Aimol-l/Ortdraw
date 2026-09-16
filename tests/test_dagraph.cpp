#include <QtTest>
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "port/Port.hpp"

static BaseNode* makeNode() { return new BaseNode(); }

static Port* addPort(BaseNode* n, PortType t, DataType d) {
    auto* p = new Port("p", t, d, QPointF(0, 0), n);
    if (t == PortType::Input) n->getInPorts().append(p);
    else n->getOutPorts().append(p);
    return p;
}

class TestDAGraph : public QObject {
    Q_OBJECT
private slots:
    void addNodeRejectsDuplicate() {
        DAGraph g;
        auto* n = makeNode();
        QVERIFY(g.addNode(n));
        QVERIFY(!g.addNode(n));
        delete n;
    }
    void addEdgeSetsConnected() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(out, in));
        QVERIFY(out->isConnected());
        QVERIFY(in->isConnected());
        QCOMPARE(g.getAllEdges().size(), 1);
        delete a; delete b;
    }
    void addEdgeRejectsCycle() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* a_in = addPort(a, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(!g.addEdge(b_out, a_in));
        QCOMPARE(g.getAllEdges().size(), 1);
        delete a; delete b;
    }
    void addEdgeRejectsUnknownNode() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(!g.addEdge(out, in));
        delete a; delete b;
    }
    void addEdgeRejectsTypeMismatch() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Float);
        QVERIFY(!g.addEdge(out, in));
        delete a; delete b;
    }
    void removeEdgeResetsConnected() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(out, in));
        QVERIFY(g.removeEdge(out, in));
        QVERIFY(!out->isConnected());
        QVERIFY(!in->isConnected());
        QCOMPARE(g.getAllEdges().size(), 0);
        delete a; delete b;
    }
    void removeNodeClearsEdges() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode(); auto* c = makeNode();
        g.addNode(a); g.addNode(b); g.addNode(c);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* c_in = addPort(c, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(g.addEdge(b_out, c_in));
        QVERIFY(g.removeNode(b));
        QCOMPARE(g.getAllEdges().size(), 0);
        QCOMPARE(g.getAllNodes().size(), 2);
        delete a; delete b; delete c;
    }
    void removeNodeUnknownReturnsFalse() {
        DAGraph g;
        auto* n = makeNode();
        QVERIFY(!g.removeNode(n));
        delete n;
    }
    void removeEdgeMissingReturnsFalse() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(!g.removeEdge(out, in));
        delete a; delete b;
    }
    void outputFanOutKeepsConnectedUntilLastEdge() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode(); auto* c = makeNode();
        g.addNode(a); g.addNode(b); g.addNode(c);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* c_in = addPort(c, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(g.addEdge(a_out, c_in));
        QVERIFY(a_out->isConnected());
        QVERIFY(g.removeEdge(a_out, b_in));
        QVERIFY(a_out->isConnected());
        QVERIFY(!b_in->isConnected());
        QVERIFY(g.removeEdge(a_out, c_in));
        QVERIFY(!a_out->isConnected());
        delete a; delete b; delete c;
    }
    void edgesOfCollectsIncidentEdges() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode(); auto* c = makeNode();
        g.addNode(a); g.addNode(b); g.addNode(c);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* c_in = addPort(c, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(g.addEdge(b_out, c_in));
        QCOMPARE(g.edgesOf(b).size(), 2);
        QCOMPARE(g.edgesOf(a).size(), 1);
        delete a; delete b; delete c;
    }
};

QTEST_MAIN(TestDAGraph)
#include "test_dagraph.moc"
