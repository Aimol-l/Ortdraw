#include <QtTest>
#include <QQuickItem>
#include "command/CmdManager.hpp"
#include "command/AddNode.hpp"
#include "command/RemoveNode.hpp"
#include "command/AddEdge.hpp"
#include "command/RemoveEdge.hpp"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class TestCmdManager : public QObject {
    Q_OBJECT
private:
    PaintBoard* board = nullptr;
    QQuickItem* root = nullptr;
    BaseNode* nodeA = nullptr;
    BaseNode* nodeB = nullptr;
    BaseNode* nodeC = nullptr;
    Port* outA = nullptr;
    Port* inB = nullptr;

    void setup() {
        board = new PaintBoard();
        root = new QQuickItem();
        nodeA = new BaseNode();
        nodeB = new BaseNode();
        nodeC = new BaseNode();
        nodeA->setParentItem(root);
        nodeB->setParentItem(root);
        nodeC->setParentItem(root);
        outA = new Port("out", PortType::Output, DataType::Image, QPointF(0, 0), nodeA);
        inB  = new Port("in",  PortType::Input,  DataType::Image, QPointF(0, 0), nodeB);
        nodeA->getOutPorts().append(outA);
        nodeB->getInPorts().append(inB);
    }
    void teardown() {
        board->m_graph = DAGraph{};
        delete nodeA; delete nodeB; delete nodeC;
        nodeA = nodeB = nodeC = nullptr;
        outA = inB = nullptr;
        delete root; root = nullptr;
        delete board;
    }
private slots:
    void init() { setup(); }
    void cleanup() { teardown(); }

    void addNodeUndoRedoKeepsObjectAlive() {
        CmdManager m;
        QVERIFY(m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board)));
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QVERIFY(nodeA->isVisible());
        QVERIFY(m.undo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 0);
        QVERIFY(!nodeA->isVisible());
        QVERIFY(m.redo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QVERIFY(nodeA->isVisible());
    }
    void removeNodeRestoresEdges() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(m.executeCommand(std::make_unique<AddEdgeCMD>(outA, inB, board)));
        QCOMPARE(board->m_graph.getAllEdges().size(), 1);
        QVERIFY(m.executeCommand(std::make_unique<RemoveNodeCMD>(nodeB, board)));
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QCOMPARE(board->m_graph.getAllEdges().size(), 0);
        QVERIFY(!inB->isConnected());
        QVERIFY(m.undo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 2);
        QCOMPARE(board->m_graph.getAllEdges().size(), 1);
        QVERIFY(inB->isConnected());
        QVERIFY(outA->isConnected());
    }
    void addEdgeUndoAllowsReconnect() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(m.executeCommand(std::make_unique<AddEdgeCMD>(outA, inB, board)));
        QVERIFY(m.undo());
        QVERIFY(!inB->isConnected());
        QVERIFY(board->m_graph.addEdge(outA, inB));
    }
    void redoHistoryTruncatedByNewCommand() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(!m.canRedo());
        QVERIFY(m.undo());
        QVERIFY(m.canRedo());
        QVERIFY(m.executeCommand(std::make_unique<AddNodeCMD>(nodeC, board)));
        QVERIFY(!m.canRedo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 2);
    }
    void undoRedoOnEmptyStackReturnsFalse() {
        CmdManager m;
        QVERIFY(!m.undo());
        QVERIFY(!m.redo());
        QVERIFY(!m.canUndo());
        QVERIFY(!m.canRedo());
    }
    void removeNodeRedoRemovesEdgesAgain() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        m.executeCommand(std::make_unique<AddEdgeCMD>(outA, inB, board));
        QVERIFY(m.executeCommand(std::make_unique<RemoveNodeCMD>(nodeB, board)));
        QCOMPARE(board->m_graph.getAllEdges().size(), 0);
        QVERIFY(m.undo());
        QCOMPARE(board->m_graph.getAllEdges().size(), 1);
        QVERIFY(m.redo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QCOMPARE(board->m_graph.getAllEdges().size(), 0);
        QVERIFY(!inB->isConnected());
    }
    void failedExecuteDoesNotClobberRedo() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(m.undo());
        QVERIFY(m.canRedo());
        QVERIFY(!m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board)));
        QVERIFY(m.canRedo());
        QVERIFY(m.redo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 2);
    }
};

QTEST_MAIN(TestCmdManager)
#include "test_cmdmanager.moc"
