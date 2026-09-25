#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <memory>
#include <opencv2/imgcodecs.hpp>

#include "NodeManager.h"
#include "PaintBoard.h"
#include "Settings.h"
#include "engine/BuiltinExecutors.hpp"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include "node/Resize.hpp"

// 自动重算：设置开关、防抖合并、参数/撤销触发、运行中变更重启与持久化
class TestAutoRun : public QObject {
    Q_OBJECT
private:
    PaintBoard* board = nullptr;
    NodeManager* nm = nullptr;
    QString m_img;
    std::unique_ptr<QTemporaryFile> m_settings;
    std::unique_ptr<QTemporaryDir> m_dir;

    // 写一张真实图片，保证 ImageLoad 执行器能成功求值
    QString makeImage() {
        const QString path = m_dir->filePath("img.png");
        const cv::Mat src(10, 20, CV_8UC3, cv::Scalar(10, 20, 30)); // 宽20 高10
        if (!cv::imwrite(path.toStdString(), src)) return {};
        return path;
    }

    BaseNode* findNode(const QString& type) {
        for (BaseNode* n : board->m_graph.getAllNodes())
            if (n->typeName() == type) return n;
        return nullptr;
    }

    // 搭一条 load -> resize -> show 的图（三个节点均可成功求值）
    void buildPipeline() {
        auto* load = new ImageLoadNode();
        load->setPath(m_img);
        auto* resize = new ResizeNode();
        resize->setOutWidth(40);
        resize->setOutHeight(30);
        auto* show = new ImageShowNode();
        QVERIFY(nm->createNode(load));
        QVERIFY(nm->createNode(resize));
        QVERIFY(nm->createNode(show));
        QVERIFY(nm->addEdgeByUuid(load->uuid().toString(), 0,
                                  resize->uuid().toString(), 0));
        QVERIFY(nm->addEdgeByUuid(resize->uuid().toString(), 0,
                                  show->uuid().toString(), 0));
    }

    void setup() {
        board = new PaintBoard();
        nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        nm->setPaintBoard(board);
    }
    void teardown() {
        nm->cancelRun();
        QTRY_VERIFY(!nm->engineRunning());
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
        // 隔离 Settings 单例到临时 INI，避免污染真实配置
        m_settings = std::make_unique<QTemporaryFile>();
        QVERIFY(m_settings->open());
        qputenv("ORTDRAW_SETTINGS_PATH", m_settings->fileName().toLocal8Bit());
        Settings::settings()->setAutoRun(false);
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_img = makeImage();
        QVERIFY(!m_img.isEmpty());
    }
    void cleanupTestCase() {
        qunsetenv("ORTDRAW_SETTINGS_PATH");
    }

    // 默认关闭：结构变更不应触发任何求值
    void disabled_noAutoRun() {
        Settings::settings()->setAutoRun(false);
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        buildPipeline();
        QTest::qWait(500); // 超过防抖窗口
        QCOMPARE(triggered.count(), 0);
        QVERIFY(!nm->engineRunning());
        QVERIFY(!nm->queueHasResult());
    }

    // 开启后：一批结构变更经 300ms 防抖合并为一次求值
    void enabled_triggersOncePerBurst() {
        Settings::settings()->setAutoRun(true);
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        buildPipeline(); // 3 次 createNode + 2 次 addEdge
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(!nm->engineRunning());
        QCOMPARE(nm->queueDone(), 3); // load / resize / show 均成功
    }

    // 连续快速变更只跑一次（防抖合并）
    void enabled_debounceCoalescesRapidEdits() {
        Settings::settings()->setAutoRun(true);
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        buildPipeline();
        auto* load = qobject_cast<ImageLoadNode*>(findNode("ImageLoad"));
        auto* resize = qobject_cast<ResizeNode*>(findNode("Resize"));
        QVERIFY(load && resize);
        load->setPath(m_img);
        nm->commitNodeParams(load->uuid());
        resize->setOutWidth(48);
        nm->commitNodeParams(resize->uuid());
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(!nm->engineRunning());
        QCOMPARE(nm->queueDone(), 3);
    }

    // 撤销 / 重做属于结构变更，同样触发
    void enabled_undoRedoTrigger() {
        Settings::settings()->setAutoRun(true);
        buildPipeline();
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(!nm->engineRunning());

        QVERIFY(nm->undo()); // 撤销最后一条连线
        QTRY_COMPARE(triggered.count(), 2);
        QTRY_VERIFY(!nm->engineRunning());

        QVERIFY(nm->redo());
        QTRY_COMPARE(triggered.count(), 3);
        QTRY_VERIFY(!nm->engineRunning());
    }

    // 手动运行中又来变更：取消并以最新图重启，最终恰好多一次且结果一致
    void enabled_changeDuringRunRestarts() {
        Settings::settings()->setAutoRun(true);
        buildPipeline();
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(!nm->engineRunning());

        QVERIFY(nm->run());
        auto* resize = qobject_cast<ResizeNode*>(findNode("Resize"));
        QVERIFY(resize);
        resize->setOutWidth(56);
        nm->commitNodeParams(resize->uuid());
        // 无论取消重启还是并入新运行，最终只应再多一次触发且无残留 pending
        QTRY_COMPARE(triggered.count(), 2);
        QTRY_VERIFY(!nm->engineRunning());
        QCOMPARE(nm->queueDone(), 3);

        // 无残留：下一次变更仍能正常触发一次
        resize->setOutWidth(64);
        nm->commitNodeParams(resize->uuid());
        QTRY_COMPARE(triggered.count(), 3);
        QTRY_VERIFY(!nm->engineRunning());
        QCOMPARE(nm->queueDone(), 3);
    }

    // 纯视图变更（移动节点）不触发自动重算
    void enabled_moveDoesNotTrigger() {
        Settings::settings()->setAutoRun(true);
        buildPipeline();
        QSignalSpy triggered(nm, &NodeManager::autoRunTriggered);
        QTRY_COMPARE(triggered.count(), 1);
        QTRY_VERIFY(!nm->engineRunning());

        auto* load = qobject_cast<ImageLoadNode*>(findNode("ImageLoad"));
        QVERIFY(load);
        load->setX(load->x() + 120);
        nm->commitNodeMove(load->uuid(), load->x() - 120, load->y());
        QTest::qWait(500);
        QCOMPARE(triggered.count(), 1);
    }

    // 设置项默认值、信号与持久化
    void settingPersists() {
        {
            QTemporaryFile f;
            QVERIFY(f.open());
            Settings s(f.fileName());
            QCOMPARE(s.autoRun(), false); // 默认关闭
        }
        auto* st = Settings::settings();
        st->setAutoRun(false); // 先离开目标值，确保后续设置真正写入
        QSignalSpy changed(st, &Settings::autoRunChanged);
        st->setAutoRun(true);
        QCOMPARE(st->autoRun(), true);
        QCOMPARE(changed.count(), 1);
        st->setAutoRun(true); // 重复设置不重复发信号
        QCOMPARE(changed.count(), 1);
        st->sync();

        const QString path = QString::fromLocal8Bit(qgetenv("ORTDRAW_SETTINGS_PATH"));
        Settings reloaded(path);
        QCOMPARE(reloaded.autoRun(), true);
        reloaded.resetDefaults();
        QCOMPARE(reloaded.autoRun(), false);

        st->setAutoRun(false); // 还原，避免影响后续用例
    }
};

QTEST_MAIN(TestAutoRun)
#include "test_autorun.moc"
