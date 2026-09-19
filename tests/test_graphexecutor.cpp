#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <opencv2/imgcodecs.hpp>

#include "engine/BuiltinExecutors.hpp"
#include "engine/GraphExecutor.hpp"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include "node/Resize.hpp"
#include "node/Conv.hpp"
#include "node/Tensor.hpp"

class TestGraphExecutor : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        registerBuiltinExecutors();
    }

    void pipelineRunsInTopologicalOrder() {
        DAGraph g;
        ImageLoadNode load;
        ResizeNode resize;
        ImageShowNode show;
        QVERIFY(g.addNode(&load));
        QVERIFY(g.addNode(&resize));
        QVERIFY(g.addNode(&show));
        QVERIFY(g.addEdge(load.getOutPorts()[0], resize.getInPorts()[0]));
        QVERIFY(g.addEdge(resize.getOutPorts()[0], show.getInPorts()[0]));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("img.png");
        const cv::Mat src(10, 20, CV_8UC3, cv::Scalar(10, 20, 30));
        QVERIFY(cv::imwrite(path.toStdString(), src));
        load.setPath(path);
        resize.setOutWidth(40);
        resize.setOutHeight(30);

        GraphExecutor ex;
        ex.setGraph(&g);
        QSignalSpy finished(&ex, &GraphExecutor::runFinished);
        QSignalSpy nodeSpy(&ex, &GraphExecutor::nodeFinished);
        QSignalSpy imageSpy(&ex, &GraphExecutor::nodeImageReady);

        QVERIFY(ex.run());
        QVERIFY(!ex.run()); // 运行中拒绝再次启动

        QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0, 10000);
        QCOMPARE(finished.count(), 1);
        QVERIFY(finished.takeFirst().at(0).toBool());

        QTRY_COMPARE(nodeSpy.count(), 3);
        for (const QVariantList& args : nodeSpy) {
            QVERIFY2(args.at(1).toBool(), qPrintable(args.at(2).toString()));
        }
        // 拓扑顺序：load → resize → show
        QCOMPARE(nodeSpy.at(0).at(0).toString(), load.uuid().toString());
        QCOMPARE(nodeSpy.at(1).at(0).toString(), resize.uuid().toString());
        QCOMPARE(nodeSpy.at(2).at(0).toString(), show.uuid().toString());

        QTRY_COMPARE(imageSpy.count(), 3);
        QVERIFY(!imageSpy.at(0).at(1).value<QImage>().isNull());
        QVERIFY(!ex.running());
    }

    void tensorConvPipelineProducesImage() {
        DAGraph g;
        ImageLoadNode load;
        TensorNode tensor;
        ConvNode conv;
        ImageShowNode show;
        QVERIFY(g.addNode(&load));
        QVERIFY(g.addNode(&tensor));
        QVERIFY(g.addNode(&conv));
        QVERIFY(g.addNode(&show));
        // 卷积输入端口约定：0 = 图像, 1 = 卷积核
        QVERIFY(g.addEdge(load.getOutPorts()[0], conv.getInPorts()[0]));
        QVERIFY(g.addEdge(tensor.getOutPorts()[0], conv.getInPorts()[1]));
        QVERIFY(g.addEdge(conv.getOutPorts()[0], show.getInPorts()[0]));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("img.png");
        const cv::Mat src(8, 8, CV_8U);
        QVERIFY(cv::imwrite(path.toStdString(), src));
        load.setPath(path);

        GraphExecutor ex;
        ex.setGraph(&g);
        QSignalSpy finished(&ex, &GraphExecutor::runFinished);
        QSignalSpy nodeSpy(&ex, &GraphExecutor::nodeFinished);
        QSignalSpy imageSpy(&ex, &GraphExecutor::nodeImageReady);

        QVERIFY(ex.run());
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0, 10000);
        QVERIFY(finished.takeFirst().at(0).toBool());

        QTRY_COMPARE(nodeSpy.count(), 4);
        for (const QVariantList& args : nodeSpy)
            QVERIFY2(args.at(1).toBool(), qPrintable(args.at(2).toString()));

        QTRY_VERIFY_WITH_TIMEOUT(imageSpy.count() >= 3, 10000);
        bool convImage = false, showImage = false;
        for (const QVariantList& args : imageSpy) {
            const QString u = args.at(0).toString();
            const bool valid = !args.at(1).value<QImage>().isNull();
            if (valid && u == conv.uuid().toString()) convImage = true;
            if (valid && u == show.uuid().toString()) showImage = true;
        }
        QVERIFY2(convImage, "Conv 未产出图像");
        QVERIFY2(showImage, "ImageShow 未收到图像");
        QVERIFY(!ex.running());
    }

    void errorPropagatesToDownstream() {
        DAGraph g;
        ImageLoadNode load;
        ResizeNode resize;
        ImageShowNode show;
        QVERIFY(g.addNode(&load));
        QVERIFY(g.addNode(&resize));
        QVERIFY(g.addNode(&show));
        QVERIFY(g.addEdge(load.getOutPorts()[0], resize.getInPorts()[0]));
        QVERIFY(g.addEdge(resize.getOutPorts()[0], show.getInPorts()[0]));
        load.setPath(QStringLiteral("/no/such/file.png"));

        GraphExecutor ex;
        ex.setGraph(&g);
        QSignalSpy finished(&ex, &GraphExecutor::runFinished);
        QSignalSpy nodeSpy(&ex, &GraphExecutor::nodeFinished);

        QVERIFY(ex.run());
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() > 0, 10000);
        QCOMPARE(finished.count(), 1);
        QVERIFY(!finished.takeFirst().at(0).toBool());

        QTRY_COMPARE(nodeSpy.count(), 3);
        QCOMPARE(nodeSpy.at(0).at(0).toString(), load.uuid().toString());
        QVERIFY(!nodeSpy.at(0).at(1).toBool());
        QVERIFY(!nodeSpy.at(1).at(1).toBool());
        QVERIFY(!nodeSpy.at(2).at(1).toBool());
        QCOMPARE(nodeSpy.at(1).at(2).toString(), QStringLiteral("上游节点失败"));
    }
};

QTEST_MAIN(TestGraphExecutor)
#include "test_graphexecutor.moc"
