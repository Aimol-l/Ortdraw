#include <QtTest>
#include <variant>
#include <vector>
#include "node/OnnxInfer.hpp"
#include "engine/executors/OnnxInferExecutor.hpp"

class TestOnnxNodes : public QObject {
    Q_OBJECT
private slots:
    void portsFromMetadata() {
        OnnxInferNode n;
        n.setModelPath(QString(ORTDRAW_TEST_DATA_DIR) + "/add_dynamic.onnx");
        QVERIFY2(n.modelError().isEmpty(), qPrintable(n.modelError()));
        QCOMPARE(n.getInPorts().size(), 2);
        QCOMPARE(n.getOutPorts().size(), 1);
        QCOMPARE(n.getInPorts()[0]->name(), QString("A"));
        QCOMPARE(n.getOutPorts()[0]->name(), QString("C"));
        QCOMPARE(n.getInPorts()[0]->dataType(), DataType::Tensor);
    }

    void executorRunsAdd() {
        std::vector<float> a(12, 2.0f), b(12, 3.0f);
        std::vector<int64_t> sh{3, 4};
        Tensor ta(a, sh), tb(b, sh);
        OnnxInferExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"modelPath", QString(ORTDRAW_TEST_DATA_DIR) + "/add.onnx"}, {"device", "cpu"}},
            QVector<NodeData>{ta, tb});
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<Tensor>(r.outputs[0]));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int64_t(t.shape()[0]), int64_t(3));
        QCOMPARE(int64_t(t.shape()[1]), int64_t(4));
    }

    void executorMissingModelFails() {
        OnnxInferExecutor ex;
        QVERIFY(!ex.execute({}, QVariantMap{{"modelPath", "/no/such.onnx"}}, {}).ok);
    }
};

QTEST_MAIN(TestOnnxNodes)
#include "test_onnx_nodes.moc"
