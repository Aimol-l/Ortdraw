#include <QtTest>
#include <opencv2/imgproc.hpp>

#include "engine/NodeExecutor.hpp"
#include "engine/executors/ConvExecutor.hpp"
#include "engine/executors/TensorExecutor.hpp"
#include "node/Tensor.hpp"

namespace {

cv::Mat makeGray(int rows = 8, int cols = 8) {
    cv::Mat m(rows, cols, CV_8U);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            m.at<uchar>(r, c) = uchar(((r * cols + c) * 4) % 256);
    return m;
}

} // namespace

class TestTensorNode : public QObject {
    Q_OBJECT
private slots:
    void defaultIsThreeByThreeIdentity() {
        TensorNode n;
        QCOMPARE(n.rows(), 3);
        QCOMPARE(n.cols(), 3);
        QCOMPARE(n.kernelRows(), 3);
        QCOMPARE(n.kernelCols(), 3);
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                QCOMPARE(n.value(r, c), (r == 1 && c == 1) ? 1.0 : 0.0);
        QCOMPARE(n.shapeText(), QStringLiteral("3 × 3"));
    }

    void valueRoundTrip() {
        TensorNode n;
        n.setValue(0, 2, 2.5);
        QCOMPARE(n.value(0, 2), 2.5);
        n.setValue(2, 0, -1.25);
        QCOMPARE(n.value(2, 0), -1.25);
        // 越界读取返回 0，写入被忽略
        QCOMPARE(n.value(-1, 0), 0.0);
        QCOMPARE(n.value(9, 9), 0.0);
        n.setValue(9, 9, 7.0);
        QCOMPARE(n.value(9, 9), 0.0);
    }

    void paramsRoundTrip() {
        TensorNode a;
        a.setShape(2, 3);
        a.setValue(0, 0, 1.0);
        a.setValue(1, 2, -4.0);

        const QVariantMap p = a.params();
        QCOMPARE(p.value("rows").toInt(), 2);
        QCOMPARE(p.value("cols").toInt(), 3);
        QCOMPARE(p.value("data").toList().size(), 6);

        TensorNode b;
        b.setParams(p);
        QCOMPARE(b.rows(), 2);
        QCOMPARE(b.cols(), 3);
        QCOMPARE(b.value(0, 0), 1.0);
        QCOMPARE(b.value(1, 2), -4.0);
    }

    void setShapePreservesOverlap() {
        TensorNode n;
        n.setShape(2, 3);
        n.setValue(0, 0, 5.0);
        n.setValue(0, 1, 6.0);
        n.setValue(1, 2, 7.0);

        n.setShape(3, 3);
        QCOMPARE(n.rows(), 3);
        QCOMPARE(n.cols(), 3);
        QCOMPARE(n.value(0, 0), 5.0);
        QCOMPARE(n.value(0, 1), 6.0);
        QCOMPARE(n.value(1, 2), 7.0);
        QCOMPARE(n.value(2, 2), 0.0);

        n.setShape(1, 1);
        QCOMPARE(n.rows(), 1);
        QCOMPARE(n.cols(), 1);
        QCOMPARE(n.value(0, 0), 5.0);
    }

    void presetsBehave() {
        TensorNode n;
        n.presetMean();
        for (int i = 0; i < 9; ++i) {
            const int r = i / 3, c = i % 3;
            QVERIFY(qFuzzyCompare(n.value(r, c), 1.0 / 9.0));
        }
        n.presetSharpen();
        QCOMPARE(n.value(1, 1), 5.0);
        QCOMPARE(n.value(0, 1), -1.0);
        n.presetIdentity();
        QCOMPARE(n.value(1, 1), 1.0);
        QCOMPARE(n.value(0, 1), 0.0);
    }

    void tensorExecutorProducesTensor() {
        TensorNode n;
        TensorExecutor ex;
        const ExecResult r = ex.execute({}, n.params(), {});
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 1);
        QVERIFY(std::holds_alternative<Tensor>(r.outputs[0]));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int(t.shape(0)), 3);
        QCOMPARE(int(t.shape(1)), 3);
        QCOMPARE(t.dtype(), via::DataType::FLOAT32);
    }

    void tensorExecutorRejectsMismatchedData() {
        TensorExecutor ex;
        QVariantMap p{ { "rows", 2 }, { "cols", 2 }, { "data", QVariantList{ 1.0, 2.0, 3.0 } } };
        const ExecResult r = ex.execute({}, p, {});
        QVERIFY(!r.ok);
        QCOMPARE(r.error, QStringLiteral("张量数据尺寸不匹配"));
    }

    void convIdentityFromTensorPreservesImage() {
        TensorNode n;
        TensorExecutor tex;
        const ExecResult tres = tex.execute({}, n.params(), {});
        QVERIFY2(tres.ok, qPrintable(tres.error));

        ConvExecutor conv;
        const cv::Mat in = makeGray(8, 8);
        QVector<NodeData> inputs;
        inputs.push_back(in);
        inputs.push_back(tres.outputs[0]);
        const ExecResult r = conv.execute({}, {}, inputs);
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.rows, in.rows);
        QCOMPARE(out.cols, in.cols);
        cv::Mat diff;
        cv::absdiff(out, in, diff);
        QCOMPARE(cv::countNonZero(diff.reshape(1)), 0);
    }
};

QTEST_MAIN(TestTensorNode)
#include "test_tensor_node.moc"
