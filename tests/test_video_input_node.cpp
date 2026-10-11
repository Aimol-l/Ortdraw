#include <QtTest>
#include "node/VideoInput.hpp"

class TestVideoInputNode : public QObject {
    Q_OBJECT
private slots:
    // setPath 会把 frame 归零；setParams 必须在 path 之后恢复 frame
    void paramsRoundTripRestoresFrameAfterPath() {
        VideoInputNode n;
        n.setPath("/tmp/a.mkv");
        n.setFrame(42);
        QCOMPARE(n.frame(), 42);
        const QVariantMap p = n.params();
        QCOMPARE(p.value("path").toString(), QString("/tmp/a.mkv"));
        QCOMPARE(p.value("frame").toInt(), 42);

        VideoInputNode m;
        m.setParams(p);                 // 内部先 setPath（归零）再 setFrame
        QCOMPARE(m.path(), QString("/tmp/a.mkv"));
        QCOMPARE(m.frame(), 42);        // 关键：不能被 setPath 归零
    }

    // fps 钳制：非法值（0 / 负数 / 超限）被夹到合法区间
    void fpsIsClamped() {
        VideoInputNode n;
        n.setFps(0);      QCOMPARE(n.fps(), 1);
        n.setFps(-5);     QCOMPARE(n.fps(), 1);
        n.setFps(999);    QCOMPARE(n.fps(), 120);
        n.setFps(25);     QCOMPARE(n.fps(), 25);
    }

    // loop / frame 下界
    void loopAndFrameFloor() {
        VideoInputNode n;
        n.setLoop(false); QCOMPARE(n.loop(), false);
        n.setLoop(true);  QCOMPARE(n.loop(), true);
        n.setFrame(-3);   QCOMPARE(n.frame(), 0);
    }

    // 参数缺省时有合理默认值
    void defaults() {
        VideoInputNode n;
        QCOMPARE(n.fps(), 25);
        QCOMPARE(n.loop(), true);
        QCOMPARE(n.frame(), 0);
        QVERIFY(n.path().isEmpty());
        QCOMPARE(n.frameCount(), 0);
        QVERIFY(!n.videoValid());
    }
};

QTEST_MAIN(TestVideoInputNode)
#include "test_video_input_node.moc"
