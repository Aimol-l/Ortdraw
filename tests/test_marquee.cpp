#include <QtTest>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <memory>

#include "NodeManager.h"
#include "PaintBoard.h"
#include "Settings.h"
#include "engine/BuiltinExecutors.hpp"
#include "node/ImageLoad.hpp"
#include "node/Resize.hpp"
#include "node/Blur.hpp"

// 拉选（框选）多节点：相交命中、全量替换、Ctrl 追加与门控、选框生命周期
class TestMarquee : public QObject {
    Q_OBJECT
private:
    PaintBoard* board = nullptr;
    NodeManager* nm = nullptr;
    ImageLoadNode* load = nullptr;
    ResizeNode* resize = nullptr;
    BlurNode* blur = nullptr;

    void setup() {
        board = new PaintBoard();
        nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        nm->setPaintBoard(board);
        // 世界坐标布局（运行时由 QML 设置尺寸，测试显式给定）
        load   = new ImageLoadNode(); load->setX(0);   load->setY(0);
        resize = new ResizeNode();    resize->setX(400); resize->setY(0);
        blur   = new BlurNode();      blur->setX(0);     blur->setY(400);
        for (BaseNode* n : {static_cast<BaseNode*>(load),
                           static_cast<BaseNode*>(resize),
                           static_cast<BaseNode*>(blur)}) {
            n->setWidth(220);
            n->setHeight(120);
            QVERIFY(board->m_graph.addNode(n));
        }
        Settings::settings()->setCtrlMultiSelect(true);
    }
    void teardown() {
        nm->endMarquee();   // 收尾单例的框选状态，避免跨用例残留
        board->m_graph = DAGraph{};
        for (BaseNode* n : {static_cast<BaseNode*>(load),
                           static_cast<BaseNode*>(resize),
                           static_cast<BaseNode*>(blur)}) delete n;
        load = nullptr;
        resize = nullptr;
        blur = nullptr;
        nm->setPaintBoard(nullptr);
        delete board;
        board = nullptr;
        nm = nullptr;
    }

private slots:
    void init() { setup(); }
    void cleanup() { teardown(); }

    void initTestCase() {
        registerBuiltinExecutors();
        m_settings = std::make_unique<QTemporaryFile>();
        QVERIFY(m_settings->open());
        qputenv("ORTDRAW_SETTINGS_PATH", m_settings->fileName().toLocal8Bit());
        Settings::settings()->setCtrlMultiSelect(true);
    }
    void cleanupTestCase() {
        qunsetenv("ORTDRAW_SETTINGS_PATH");
    }

    // 相交即选中：选框 0..250 x 0..130 只碰到 load
    void intersectSelectsTouchedNodes() {
        nm->beginMarquee(-10, -10, false);
        nm->updateMarquee(250, 130);
        QVERIFY(load->selected());
        QVERIFY(!resize->selected());
        QVERIFY(!blur->selected());
        QCOMPARE(nm->selectedNode(), load);   // 恰好一个 → 指向它
    }

    // 部分相交也算命中：选框 200..430 与 load 右缘、resize 左缘都相交
    void partialIntersectCounts() {
        nm->beginMarquee(200, 0, false);
        nm->updateMarquee(430, 120);
        QVERIFY(load->selected());
        QVERIFY(resize->selected());
        QVERIFY(!blur->selected());
        QCOMPARE(nm->selectedNode(), nullptr); // 两个选中 → 无单一选中
    }

    // 非 additive 框选先清空旧选择，再全量替换
    void replaceClearsPrevious() {
        load->setSelected(true);
        nm->beginMarquee(400, 0, false);
        QVERIFY(!load->selected());           // begin 即清空
        nm->updateMarquee(620, 120);
        QVERIFY(resize->selected());
        QVERIFY(!load->selected());
        QVERIFY(!blur->selected());
    }

    // Ctrl 追加：保留已选中的，只把新碰到的加进来
    void ctrlAdditiveKeepsExisting() {
        load->setSelected(true);
        nm->beginMarquee(400, 0, true);
        QVERIFY(load->selected());            // begin 不清空
        nm->updateMarquee(620, 120);
        QVERIFY(load->selected());
        QVERIFY(resize->selected());
        QVERIFY(!blur->selected());
    }

    // ctrlMultiSelect 关闭时 additive 失效，退化为全量替换
    void ctrlAdditiveGatedBySetting() {
        Settings::settings()->setCtrlMultiSelect(false);
        load->setSelected(true);
        nm->beginMarquee(400, 0, true);
        QVERIFY(!load->selected());
        nm->updateMarquee(620, 120);
        QVERIFY(resize->selected());
        QVERIFY(!load->selected());
        Settings::settings()->setCtrlMultiSelect(true);
    }

    // 选框生命周期：begin/update 给出世界矩形，end 后清除
    void marqueeRectLifecycle() {
        nm->beginMarquee(30, 40, false);
        nm->updateMarquee(120, 160);
        QVERIFY(board->marqueeVisible());
        QCOMPARE(board->marquee(), QRectF(30, 40, 90, 120).normalized());
        // 反向拖动（右下 → 左上）归一化
        nm->updateMarquee(-20, -10);
        QCOMPARE(board->marquee(), QRectF(-20, -10, 50, 50).normalized());
        nm->endMarquee();
        QVERIFY(!board->marqueeVisible());
    }

    // 未挂画布时所有入口安全返回
    void nullBoardSafe() {
        nm->setPaintBoard(nullptr);
        nm->beginMarquee(0, 0, false);
        nm->updateMarquee(50, 50);
        nm->endMarquee();
    }

    // updateMarquee 在未 begin 时忽略
    void updateWithoutBeginIgnored() {
        load->setSelected(false);
        nm->updateMarquee(100, 100);
        QVERIFY(!load->selected());
        QVERIFY(!board->marqueeVisible());
    }

    // 拉选多个后 removeNode() 一次删除全部选中节点（Delete 键走同一路径）
    void marqueeMultiDelete() {
        nm->beginMarquee(-50, -50, false);
        nm->updateMarquee(1200, 600);
        QVERIFY(load->selected());
        QVERIFY(resize->selected());
        QVERIFY(blur->selected());
        nm->endMarquee();
        QVERIFY(nm->removeNode());
        QCOMPARE(nm->nodeCount(), 0);
        QVERIFY(!load->selected());
        QVERIFY(!resize->selected());
        QVERIFY(!blur->selected());
        // 撤销可恢复（每个节点一条命令，逐条撤销）
        for (int i = 0; i < 3; ++i)
            QVERIFY(nm->undo());
        QCOMPARE(nm->nodeCount(), 3);
    }

    // 多选整体拖拽（模拟 QML：被拖节点直接赋值 + nodeMoveEvent 传 delta）：
    // 其余选中节点整体跟随（含端口），一次 undo 整组还原
    void groupMoveViaDragEvents() {
        nm->beginMarquee(-10, -10, false);
        nm->updateMarquee(500, 200);   // 选中 load + resize（不含 blur）
        QVERIFY(load->selected());
        QVERIFY(resize->selected());

        Port* resizeOut = resize->getOutPorts().first();
        const QPointF rp0 = resizeOut->position();

        // 第一段位移：QML 设被拖节点位置，nodeMoveEvent 同步
        load->setX(50);
        load->setY(30);
        nm->nodeMoveEvent(load->uuid(), 50, 30);
        // 第二段位移（增量）
        load->setX(70);
        load->setY(40);
        nm->nodeMoveEvent(load->uuid(), 20, 10);

        QCOMPARE(load->x(), 70.0);
        QCOMPARE(load->y(), 40.0);
        QCOMPARE(resize->x(), 470.0);      // 400 + 50 + 20 整体跟随
        QCOMPARE(resize->y(), 40.0);       // 0 + 30 + 10
        QCOMPARE(blur->x(), 0.0);          // 未选中不动
        QCOMPARE(blur->y(), 400.0);
        // 其他节点的端口也跟随
        QCOMPARE(resizeOut->position().x() - rp0.x(), 70.0);
        QCOMPARE(resizeOut->position().y() - rp0.y(), 40.0);

        // 提交：一条组命令，一次 undo 整组还原
        nm->commitNodeMove(load->uuid(), 0, 0);
        QVERIFY(nm->undo());
        QCOMPARE(load->x(), 0.0);
        QCOMPARE(load->y(), 0.0);
        QCOMPARE(resize->x(), 400.0);
        QCOMPARE(resize->y(), 0.0);
        QCOMPARE(resizeOut->position().x(), rp0.x());
        QCOMPARE(resizeOut->position().y(), rp0.y());
    }

    // 拖未选中节点：只移动该节点，不影响选择集；单节点命令可撤销
    void moveUnselectedViaDragEvents() {
        nm->beginMarquee(-10, -10, false);
        nm->updateMarquee(500, 200);   // load + resize 选中
        blur->setX(10);
        blur->setY(410);
        nm->nodeMoveEvent(blur->uuid(), 10, 10);
        QCOMPARE(load->x(), 0.0);
        QCOMPARE(resize->x(), 400.0);
        QCOMPARE(blur->x(), 10.0);

        nm->commitNodeMove(blur->uuid(), 0, 400);
        QVERIFY(nm->undo());
        QCOMPARE(blur->x(), 0.0);
        QCOMPARE(blur->y(), 400.0);
    }

    // 无多选时单节点拖拽：行为与之前一致（单条 MoveNodeCMD）
    void singleNodeMoveViaDragEvents() {
        nm->beginMarquee(-10, -10, false);
        nm->updateMarquee(250, 130);   // 只选中 load
        load->setX(5);
        load->setY(5);
        nm->nodeMoveEvent(load->uuid(), 5, 5);
        QCOMPARE(resize->x(), 400.0);
        nm->commitNodeMove(load->uuid(), 0, 0);
        QVERIFY(nm->undo());
        QCOMPARE(load->x(), 0.0);
        QCOMPARE(load->y(), 0.0);
    }

private:
    std::unique_ptr<QTemporaryFile> m_settings;
};

QTEST_MAIN(TestMarquee)
#include "test_marquee.moc"
