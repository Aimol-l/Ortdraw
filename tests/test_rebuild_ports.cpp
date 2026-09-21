#include <QtTest>
#include "node/BaseNode.hpp"
#include "utils/DAGraph.hpp"

class DummyRebuildNode : public BaseNode {
    Q_OBJECT
public:
    QString typeName() const override { return "DummyRebuild"; }
    DummyRebuildNode() {
        m_input_ports.push_back(new Port("in0", PortType::Input, DataType::Tensor, {}, this));
        m_output_ports.push_back(new Port("out0", PortType::Output, DataType::Tensor, {}, this));
    }
};

class TestRebuildPorts : public QObject {
    Q_OBJECT
private slots:
    void rebuildReplacesPorts() {
        DummyRebuildNode n;
        QCOMPARE(n.getInPorts().size(), 1);
        QSignalSpy inSpy(&n, &BaseNode::inputPortsChanged);
        QSignalSpy outSpy(&n, &BaseNode::outputPortsChanged);
        n.rebuildPorts({{"a", DataType::Tensor}, {"b", DataType::Image}},
                       {{"out0", DataType::Tensor}});
        QCOMPARE(n.getInPorts().size(), 2);
        QCOMPARE(n.getOutPorts().size(), 1);
        QCOMPARE(n.getInPorts()[1]->name(), QString("b"));
        QCOMPARE(n.getInPorts()[1]->dataType(), DataType::Image);
        QCOMPARE(n.getOutPorts()[0]->name(), QString("out0"));
        QCOMPARE(inSpy.count(), 1);
        QCOMPARE(outSpy.count(), 1);
    }

    // 复现并锁定 rebuildNodePorts 的安全顺序契约：先删边，再重建端口，
    // 保证删除旧端口后图中不再有任何边引用它，且新端口可重新连接。
    void edgesRemovedBeforePortsDeleted() {
        DAGraph g;
        DummyRebuildNode a, b;
        g.addNode(&a);
        g.addNode(&b);
        QVERIFY(g.addEdge(a.getOutPorts()[0], b.getInPorts()[0]));
        QCOMPARE(g.getAllEdges().size(), 1);

        // 模拟 rebuildNodePorts：按值复制边列表后先删边（避免迭代器失效）
        const QVector<Edge> edges = g.getAllEdges();
        for (const Edge& e : edges)
            g.removeEdge(e.start_port, e.stop_port);
        QCOMPARE(g.getAllEdges().size(), 0);

        a.rebuildPorts({{"in", DataType::Tensor}}, {{"out", DataType::Tensor}});
        QCOMPARE(g.getAllEdges().size(), 0);
        QVERIFY(g.addEdge(a.getOutPorts()[0], b.getInPorts()[0]));
        QCOMPARE(g.getAllEdges().size(), 1);
    }
};
QTEST_MAIN(TestRebuildPorts)
#include "test_rebuild_ports.moc"
