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

    void roleNamesMatchQmlContract() {
        ExecQueueModel m;
        const QHash<int, QByteArray> roles = m.roleNames();
        QCOMPARE(roles.size(), 7);
        QCOMPARE(roles.value(ExecQueueModel::UuidRole), QByteArray("uuid"));
        QCOMPARE(roles.value(ExecQueueModel::NameRole), QByteArray("name"));
        QCOMPARE(roles.value(ExecQueueModel::StatusRole), QByteArray("status"));
        QCOMPARE(roles.value(ExecQueueModel::MsRole), QByteArray("ms"));
        QCOMPARE(roles.value(ExecQueueModel::ErrorRole), QByteArray("error"));
        QCOMPARE(roles.value(ExecQueueModel::GroupRole), QByteArray("group"));
        QCOMPARE(roles.value(ExecQueueModel::GroupColorRole), QByteArray("groupColor"));
    }

    void invalidIndexReturnsEmpty() {
        ExecQueueModel m;
        QVERIFY(!m.data(QModelIndex(), ExecQueueModel::StatusRole).isValid());
        QVERIFY(!m.data(m.index(5), ExecQueueModel::StatusRole).isValid());
    }

    void countDoneExcludesRunning() {
        ExecQueueModel m;
        m.addRunning("a", "A");
        m.addRunning("b", "B");
        m.finishNode("a", int(NodeStatus::Ok), QString(), 1);
        QCOMPARE(m.countDone(), 1);
    }

    void groupRolesAndFilteredCounts() {
        ExecQueueModel m;
        m.addRunning("a", "A", 0, "#0969da");
        m.addRunning("b", "B", 0, "#0969da");
        m.addRunning("c", "C", 1, "#8250df");
        m.finishNode("a", int(NodeStatus::Ok), QString(), 1);
        m.finishNode("b", int(NodeStatus::Failed), QStringLiteral("e"), 2);
        m.finishNode("c", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);

        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupRole).toInt(), 0);
        QCOMPARE(m.data(m.index(2), ExecQueueModel::GroupRole).toInt(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupColorRole).toString(), QString("#0969da"));
        QVERIFY(m.roleNames().values().contains(QByteArray("group")));
        QVERIFY(m.roleNames().values().contains(QByteArray("groupColor")));

        QCOMPARE(m.countDone(), 3);
        QCOMPARE(m.countDone(0), 2);
        QCOMPARE(m.countDone(1), 1);
        QCOMPARE(m.countFailed(0), 1);
        QCOMPARE(m.countFailed(1), 0);
        QCOMPARE(m.countSkipped(1), 1);
    }

    void defaultArgRowIsUngrouped() {
        ExecQueueModel m;
        m.addRunning("x", "X");
        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupRole).toInt(), -1);
        QVERIFY(m.data(m.index(0), ExecQueueModel::GroupColorRole).toString().isEmpty());
        m.finishNode("x", int(NodeStatus::Failed), QStringLiteral("e"), 1);
        QCOMPARE(m.countDone(), 1);
        QCOMPARE(m.countFailed(), 1);
        QCOMPARE(m.countDone(0), 0);
        QCOMPARE(m.countFailed(0), 0);
    }

    void addFinishedPopulatesGroupRoles() {
        ExecQueueModel m;
        m.addFinished("y", "Y", int(NodeStatus::Failed), QStringLiteral("e"), 3, 1, "#8250df");
        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupRole).toInt(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupColorRole).toString(), QString("#8250df"));
        QCOMPARE(m.countFailed(1), 1);
    }

    void perGroupCountDoneExcludesRunning() {
        ExecQueueModel m;
        m.addRunning("r", "R", 0, "#0969da");
        m.addRunning("f", "F", 0, "#0969da");
        m.finishNode("f", int(NodeStatus::Ok), QString(), 1);
        m.addRunning("o", "O", 1, "#8250df");
        m.finishNode("o", int(NodeStatus::Ok), QString(), 2);
        QCOMPARE(m.countDone(0), 1);
        QCOMPARE(m.countDone(1), 1);
        QCOMPARE(m.countDone(), 2);
    }

    void countCancelledPerGroup() {
        ExecQueueModel m;
        m.addFinished("c", "C", int(NodeStatus::Cancelled), QString(), 4, 1, "#8250df");
        QCOMPARE(m.countCancelled(1), 1);
        QCOMPARE(m.countCancelled(0), 0);
        QCOMPARE(m.countCancelled(), 1);
    }
};

QTEST_MAIN(TestExecQueue)
#include "test_execqueue.moc"
