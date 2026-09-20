#include <QtTest>
#include "utils/QueueGroups.hpp"

class TestQueueGroups : public QObject {
    Q_OBJECT
private slots:
    void twoChains() {
        // 0->1->2   3->4
        const QStringList names = {"加载图片", "灰度化", "高斯模糊", "裁剪", "图片显示"};
        const QVector<QPair<int,int>> edges = {{0,1},{1,2},{3,4}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 2);
        QCOMPARE(gs.at(0).count, 3);
        QCOMPARE(gs.at(0).name, QStringLiteral("加载图片…"));
        QCOMPARE(gs.at(1).count, 2);
        QCOMPARE(gs.at(1).name, QStringLiteral("裁剪…"));
        QCOMPARE(idx, (QVector<int>{0,0,0,1,1}));
        QVERIFY(gs.at(0).color.isValid());
        QVERIFY(gs.at(0).color != gs.at(1).color);
    }

    void singleChain() {
        const QStringList names = {"A", "B"};
        const QVector<QPair<int,int>> edges = {{0,1}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 1);
        QCOMPARE(gs.at(0).name, QStringLiteral("A…"));
        QCOMPARE(idx, (QVector<int>{0,0}));
    }

    void isolatedNodesAreOwnGroups() {
        const QStringList names = {"A", "B", "C"};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, {}, idx);
        QCOMPARE(gs.size(), 3);
        QCOMPARE(idx, (QVector<int>{0,1,2}));
    }

    void cycleHasNoRootFallsBackToMinIndex() {
        const QStringList names = {"甲", "乙", "丙", "丁"};
        const QVector<QPair<int,int>> edges = {{0,1},{1,2},{2,0}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 2);
        QCOMPARE(gs.at(0).name, QStringLiteral("甲…"));
        QCOMPARE(gs.at(1).name, QStringLiteral("丁…"));
    }

    void convergingRootsUseMinIndexRootName() {
        const QStringList names = {"根A", "根B", "汇"};
        const QVector<QPair<int,int>> edges = {{0,2},{1,2}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 1);
        QCOMPARE(gs.at(0).name, QStringLiteral("根A…"));
        QCOMPARE(idx, (QVector<int>{0,0,0}));
    }
};

QTEST_MAIN(TestQueueGroups)
#include "test_queuegroups.moc"
