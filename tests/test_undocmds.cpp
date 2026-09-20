#include <QtTest>
#include <QQuickItem>
#include "command/MoveNodeCMD.hpp"
#include "command/ResizeNodeCMD.hpp"
#include "command/ChangeParamsCMD.hpp"
#include "command/AddNode.hpp"
#include "command/CmdManager.hpp"
#include "PaintBoard.h"
#include "node/Resize.hpp"
#include "NodeManager.h"

// 节点移动 / 缩放 / 参数名称变更的撤销重做覆盖
class TestUndoCmds : public QObject {
    Q_OBJECT
private:
    PaintBoard* board = nullptr;
    QQuickItem* root = nullptr;
    ResizeNode* node = nullptr;
    Port* out = nullptr;

    void setup() {
        board = new PaintBoard();
        root = new QQuickItem();
        node = new ResizeNode();
        node->setParentItem(root);
        out = node->getOutPorts().first();
    }
    void teardown() {
        board->m_graph = DAGraph{};
        delete node;
        node = nullptr;
        out = nullptr;
        delete root;
        root = nullptr;
        delete board;
        board = nullptr;
    }
private slots:
    void init() { setup(); }
    void cleanup() { teardown(); }

    void moveNodeUndoRedo() {
        board->m_graph.addNode(node);
        node->setX(100);
        node->setY(100);
        out->setPosition(QPointF(100, 110));
        CmdManager m;
        QVERIFY(m.executeCommand(std::make_unique<MoveNodeCMD>(
            node, QPointF(100, 100), QPointF(200, 150), board)));
        QCOMPARE(node->x(), 200.0);
        QCOMPARE(node->y(), 150.0);
        QCOMPARE(out->position().x(), 200.0);
        QCOMPARE(out->position().y(), 160.0);
        QVERIFY(m.undo());
        QCOMPARE(node->x(), 100.0);
        QCOMPARE(node->y(), 100.0);
        QCOMPARE(out->position().x(), 100.0);
        QCOMPARE(out->position().y(), 110.0);
        QVERIFY(m.redo());
        QCOMPARE(node->x(), 200.0);
        QCOMPARE(out->position().x(), 200.0);
    }

    void resizeNodeUndoRedo() {
        board->m_graph.addNode(node);
        node->setWidth(220);
        node->setHeight(120);
        out->setPosition(QPointF(220, 60));
        CmdManager m;
        QVERIFY(m.executeCommand(std::make_unique<ResizeNodeCMD>(
            node, QSizeF(220, 120), QSizeF(320, 200), board)));
        QCOMPARE(node->width(), 320.0);
        QCOMPARE(node->height(), 200.0);
        QCOMPARE(out->position().x(), 320.0);
        QCOMPARE(out->position().y(), 60.0);
        QVERIFY(m.undo());
        QCOMPARE(node->width(), 220.0);
        QCOMPARE(node->height(), 120.0);
        QCOMPARE(out->position().x(), 220.0);
        QVERIFY(m.redo());
        QCOMPARE(node->width(), 320.0);
        QCOMPARE(out->position().x(), 320.0);
    }

    void changeParamsUndoRedo() {
        board->m_graph.addNode(node);
        QCOMPARE(node->outWidth(), 224);
        const QVariantMap old_params = node->params();
        QVariantMap new_params = old_params;
        new_params["outWidth"] = 512;
        CmdManager m;
        QVERIFY(m.executeCommand(std::make_unique<ChangeParamsCMD>(
            node, old_params, new_params,
            QStringLiteral("缩放"), QStringLiteral("自定义"), board)));
        QCOMPARE(node->outWidth(), 512);
        QCOMPARE(node->name(), QStringLiteral("自定义"));
        QVERIFY(m.undo());
        QCOMPARE(node->outWidth(), 224);
        QCOMPARE(node->name(), QStringLiteral("缩放"));
        QVERIFY(m.redo());
        QCOMPARE(node->outWidth(), 512);
        QCOMPARE(node->name(), QStringLiteral("自定义"));
    }

    void commitWithoutChangeRecordsNothing() {
        auto* nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        PaintBoard live_board;
        nm->setPaintBoard(&live_board);
        auto* n = new ResizeNode();
        QVERIFY(nm->createNode(n));
        // 位置/尺寸/参数均未变化：不应产生新命令，栈顶仍是 AddNode
        nm->commitNodeMove(n->uuid(), n->x(), n->y());
        nm->commitNodeResize(n->uuid(), n->width(), n->height());
        nm->commitNodeParams(n->uuid());
        QVERIFY(nm->undo());   // 撤销 AddNode
        QCOMPARE(nm->nodeCount(), 0);
        QVERIFY(nm->redo());
        QCOMPARE(nm->nodeCount(), 1);
        nm->setPaintBoard(nullptr);
    }

    void commitMethodsProduceUndoableCommands() {
        auto* nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        PaintBoard live_board;
        nm->setPaintBoard(&live_board);
        // 节点不设父项也不释放：保持地址稳定，避免与 m_last_state 的裸指针键冲突
        auto* n = new ResizeNode();
        n->setWidth(220);
        n->setHeight(120);
        n->setX(100);
        n->setY(100);
        QVERIFY(nm->createNode(n));
        auto* o = n->getOutPorts().first();
        o->setPosition(QPointF(100, 110));

        // 移动：QML 已实时改值，提交应记录为一条可撤销命令
        n->setX(200);
        n->setY(150);
        o->movedeltaPos(QPointF(100, 50));
        nm->commitNodeMove(n->uuid(), 100, 100);
        QVERIFY(nm->undo());
        QCOMPARE(n->x(), 100.0);
        QCOMPARE(o->position().x(), 100.0);
        QVERIFY(nm->redo());
        QCOMPARE(n->x(), 200.0);
        QCOMPARE(o->position().x(), 200.0);

        // 缩放：输出端口随宽度平移
        n->setWidth(320);
        n->setHeight(300);
        o->movedeltaPos(QPointF(100, 0));
        nm->commitNodeResize(n->uuid(), 220, 120);
        QCOMPARE(n->width(), 320.0);
        QCOMPARE(o->position().x(), 300.0);
        QVERIFY(nm->undo());
        QCOMPARE(n->width(), 220.0);
        QCOMPARE(n->height(), 120.0);
        QCOMPARE(o->position().x(), 200.0);
        QVERIFY(nm->redo());
        QCOMPARE(n->width(), 320.0);
        QCOMPARE(o->position().x(), 300.0);

        // 参数变更：与上次提交基线 diff
        n->setOutWidth(512);
        nm->commitNodeParams(n->uuid());
        QCOMPARE(n->outWidth(), 512);
        QVERIFY(nm->undo());
        QCOMPARE(n->outWidth(), 224);
        QVERIFY(nm->redo());
        QCOMPARE(n->outWidth(), 512);

        // 名称变更：同样被纳入撤销历史
        n->setName(QStringLiteral("改名的缩放"));
        nm->commitNodeParams(n->uuid());
        QCOMPARE(n->name(), QStringLiteral("改名的缩放"));
        QVERIFY(nm->undo());
        QCOMPARE(n->name(), QStringLiteral("缩放"));
        QVERIFY(nm->redo());
        QCOMPARE(n->name(), QStringLiteral("改名的缩放"));

        nm->setPaintBoard(nullptr);
    }
};

QTEST_MAIN(TestUndoCmds)
#include "test_undocmds.moc"
