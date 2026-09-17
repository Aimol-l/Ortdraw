#include <QtTest>
#include <QDir>
#include <QTemporaryFile>
#include <QJsonDocument>
#include <QJsonObject>
#include "NodeManager.h"
#include "PaintBoard.h"
#include "node/ImageLoad.hpp"
#include "node/Resize.hpp"
#include "node/Blur.hpp"
#include "node/Threshold.hpp"

// 节点参数序列化 + 图文件 JSON 保存/读取
class TestGraphJson : public QObject {
    Q_OBJECT
private slots:
    void resizeParamsRoundTrip() {
        ResizeNode n;
        QCOMPARE(n.params().value("mode").toInt(), 0);
        QCOMPARE(n.params().value("outWidth").toInt(), 224);
        QCOMPARE(n.params().value("outHeight").toInt(), 224);
        QCOMPARE(n.params().value("percent").toInt(), 100);

        QVariantMap p;
        p["mode"] = 1;
        p["outWidth"] = 512;
        p["outHeight"] = 256;
        p["percent"] = 150;
        n.setParams(p);
        QCOMPARE(n.mode(), 1);
        QCOMPARE(n.outWidth(), 512);
        QCOMPARE(n.outHeight(), 256);
        QCOMPARE(n.percent(), 150);

        QVariantMap out = n.params();
        QCOMPARE(out.value("mode").toInt(), 1);
        QCOMPARE(out.value("outWidth").toInt(), 512);
        QCOMPARE(out.value("outHeight").toInt(), 256);
        QCOMPARE(out.value("percent").toInt(), 150);
    }
    void blurParamsRoundTrip() {
        BlurNode b;
        QCOMPARE(b.params().value("kernel").toInt(), 3);
        QVariantMap p; p["kernel"] = 7;
        b.setParams(p);
        QCOMPARE(b.kernel(), 7);
        QCOMPARE(b.params().value("kernel").toInt(), 7);
    }
    void thresholdParamsRoundTrip() {
        ThresholdNode t;
        QCOMPARE(t.params().value("threshold").toInt(), 128);
        QVariantMap p; p["threshold"] = 42;
        t.setParams(p);
        QCOMPARE(t.threshold(), 42);
        QCOMPARE(t.params().value("threshold").toInt(), 42);
    }
    void graphSaveLoadRoundTrip() {
        PaintBoard board;
        auto* nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        nm->setPaintBoard(&board);

        auto* load = new ImageLoadNode();
        auto* resize = new ResizeNode();
        resize->setX(400);
        resize->setY(120);
        resize->setOutWidth(320);
        resize->setOutHeight(240);
        QVERIFY(nm->createNode(load));
        QVERIFY(nm->createNode(resize));

        QVariantMap map = nm->graphToMap();
        QCOMPARE(map.value("version").toInt(), 1);
        QCOMPARE(map.value("nodes").toList().size(), 2);
        QCOMPARE(map.value("edges").toList().size(), 0);
        QVERIFY(nm->addEdgeByUuid(load->uuid().toString(), 0,
                                  resize->uuid().toString(), 0));
        map = nm->graphToMap();
        QCOMPARE(map.value("edges").toList().size(), 1);
        QCOMPARE(nm->edgeCount(), 1);

        QTemporaryFile f(QDir::tempPath() + "/ortdraw_test_XXXXXX.ortdraw");
        QVERIFY(f.open());
        QVERIFY(nm->saveGraph(f.fileName()));

        QVariantMap loaded = nm->readGraph(f.fileName());
        QCOMPARE(loaded.value("version").toInt(), 1);
        const QVariantList nodes = loaded.value("nodes").toList();
        QCOMPARE(nodes.size(), 2);
        bool foundResize = false;
        for(const QVariant& v : nodes){
            const QVariantMap m = v.toMap();
            if(m.value("type").toString() != "Resize") continue;
            foundResize = true;
            QCOMPARE(m.value("x").toDouble(), 400.0);
            QCOMPARE(m.value("y").toDouble(), 120.0);
            const QVariantMap params = m.value("params").toMap();
            QCOMPARE(params.value("outWidth").toInt(), 320);
            QCOMPARE(params.value("outHeight").toInt(), 240);
        }
        QVERIFY(foundResize);
        const QVariantList edges = loaded.value("edges").toList();
        QCOMPARE(edges.size(), 1);
        const QVariantMap e0 = edges.first().toMap();
        QCOMPARE(e0.value("fromPort").toInt(), 0);
        QCOMPARE(e0.value("toPort").toInt(), 0);
        QCOMPARE(e0.value("fromNode").toString(), load->uuid().toString());
        QCOMPARE(e0.value("toNode").toString(), resize->uuid().toString());

        // 读取的 JSON 确实是对象
        QFile raw(f.fileName());
        QVERIFY(raw.open(QIODevice::ReadOnly));
        const QJsonDocument doc = QJsonDocument::fromJson(raw.readAll());
        QVERIFY(doc.isObject());

        nm->clearGraph();
        QCOMPARE(nm->nodeCount(), 0);

        // 模拟重新打开：节点以全新 uuid 构造，再用保存的 uuid 恢复后，
        // 连线必须能按保存的 uuid 重新匹配（回归：此前会丢失所有连线）
        QString loadUuid;
        QString resizeUuid;
        for(const QVariant& v : nodes){
            const QVariantMap m = v.toMap();
            if(m.value("type").toString() == "ImageLoad") loadUuid = m.value("uuid").toString();
            if(m.value("type").toString() == "Resize")   resizeUuid = m.value("uuid").toString();
        }
        QVERIFY(!loadUuid.isEmpty());
        QVERIFY(!resizeUuid.isEmpty());
        auto* load2 = new ImageLoadNode();
        auto* resize2 = new ResizeNode();
        load2->setUuid(QUuid(loadUuid));
        resize2->setUuid(QUuid(resizeUuid));
        QCOMPARE(load2->uuid().toString(), loadUuid);
        QVERIFY(nm->createNode(load2));
        QVERIFY(nm->createNode(resize2));
        QVERIFY(nm->addEdgeByUuid(loadUuid, e0.value("fromPort").toInt(),
                                  resizeUuid, e0.value("toPort").toInt()));
        QCOMPARE(nm->edgeCount(), 1);

        nm->clearGraph();
        QCOMPARE(nm->nodeCount(), 0);
        nm->setPaintBoard(nullptr);
        delete load;
        delete resize;
        delete load2;
        delete resize2;
    }
    void readGraphMissingFileReturnsEmpty() {
        auto* nm = qobject_cast<NodeManager*>(NodeManager::instance());
        QVERIFY(nm);
        QVERIFY(nm->readGraph("/nonexistent/ortdraw-does-not-exist.ortdraw").isEmpty());
    }
};

QTEST_MAIN(TestGraphJson)
#include "test_graphjson.moc"
