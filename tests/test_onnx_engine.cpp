#include <QtTest>
#include "onnx_engine/runtime.hpp"
using namespace onnx_engine;

class TestOnnxEngine : public QObject {
    Q_OBJECT
private slots:
    void linksAndProbes() {
        (void)Runtime::cudaAvailable();                     // 必须能链接到库符号
        QCOMPARE(QString::fromLatin1(elementTypeName(ElementType::Float32)), QString("float32"));
        QCOMPARE(elementTypeSize(ElementType::Float32), 4);
    }

    void modelInfoReadsTwoInputsOneOutput() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        auto info = Runtime::instance().modelInfo(fx);
        QCOMPARE(info.inputs.size(), std::size_t(2));
        QCOMPARE(info.outputs.size(), std::size_t(1));
        for (const auto& t : info.inputs) {
            QVERIFY(t.isTensor);
            QCOMPARE(t.type, ElementType::Float32);
            for (int64_t d : t.shape) QVERIFY(d != 0);
        }
    }

    void modelInfoAllowsDynamicDims() {
        const std::string dyn = std::string(ORTDRAW_TEST_DATA_DIR) + "/add_dynamic.onnx";
        auto info = Runtime::instance().modelInfo(dyn);
        QCOMPARE(info.inputs.size(), std::size_t(2));
        QCOMPARE(info.outputs.size(), std::size_t(1));
        QCOMPARE(info.inputs[0].shape.size(), std::size_t(2));
        QCOMPARE(info.inputs[0].shape[0], int64_t(-1));   // 动态维
        QCOMPARE(info.inputs[0].shape[1], int64_t(4));
    }

    void missingFileReturnsError() {
        std::string err;
        auto s = Runtime::instance().session("/no/such/model.onnx", {}, err);
        QVERIFY(!s);
        QVERIFY(!err.empty());
    }
};
QTEST_MAIN(TestOnnxEngine)
#include "test_onnx_engine.moc"
