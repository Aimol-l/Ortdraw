#include <QtTest>
#include "ExecQueueModel.hpp"
#include "engine/NodeStatus.hpp"

class TestExecQueue : public QObject {
    Q_OBJECT
private slots:
    void addRunningAppends() {
        ExecQueueModel m;
        QSignalSpy spy(&m, &QAbstractItemModel::rowsInserted);
        m.addRunning("u1", "节点A");
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::UuidRole).toString(), QString("u1"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::NameRole).toString(), QString("节点A"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("running"));
    }

    void finishNodeUpdatesRow() {
        ExecQueueModel m;
        m.addRunning("u1", "节点A");
        m.addRunning("u2", "节点B");
        m.finishNode("u1", int(NodeStatus::Ok), QString(), 12);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("ok"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::MsRole).toInt(), 12);
        QCOMPARE(m.data(m.index(1), ExecQueueModel::StatusRole).toString(), QString("running"));
        QCOMPARE(m.countDone(), 1);
        QCOMPARE(m.countFailed(), 0);
    }

    void finishUnknownUuidIsSafe() {
        ExecQueueModel m;
        m.addRunning("u1", "节点A");
        QVERIFY(!m.finishNode("nope", int(NodeStatus::Failed), QStringLiteral("err"), 3));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("running"));
    }

    // 跳过节点从不发 nodeStarted，凭 nodeFinished 直接补建终结行
    void addFinishedAppendsTerminalRow() {
        ExecQueueModel m;
        m.addFinished("s1", "边缘检测", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::UuidRole).toString(), QString("s1"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::NameRole).toString(), QString("边缘检测"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("skipped"));
        QCOMPARE(m.countSkipped(), 1);
    }

    void countsStatuses() {
        ExecQueueModel m;
        m.addRunning("a", "A");
        m.addRunning("b", "B");
        m.addRunning("c", "C");
        m.addRunning("d", "D");
        m.finishNode("a", int(NodeStatus::Ok), QString(), 1);
        m.finishNode("b", int(NodeStatus::Failed), QStringLiteral("e"), 2);
        m.finishNode("c", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
        m.finishNode("d", int(NodeStatus::Cancelled), QString(), 5);
        QCOMPARE(m.countDone(), 4);
        QCOMPARE(m.countFailed(), 1);
        QCOMPARE(m.countSkipped(), 1);
        QCOMPARE(m.countCancelled(), 1);
    }

    void beginRunClears() {
        ExecQueueModel m;
        m.addRunning("u1", "A");
        m.beginRun();
        QCOMPARE(m.rowCount(), 0);
    }
};

QTEST_MAIN(TestExecQueue)
#include "test_execqueue.moc"
