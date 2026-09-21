#include <QtTest>
#include "node/BaseNode.hpp"

class DummyRebuildNode : public BaseNode {
    Q_OBJECT
public:
    QString typeName() const override { return "DummyRebuild"; }
    DummyRebuildNode() { m_input_ports.push_back(new Port("in0", PortType::Input, DataType::Tensor, {}, this)); }
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
};
QTEST_MAIN(TestRebuildPorts)
#include "test_rebuild_ports.moc"
