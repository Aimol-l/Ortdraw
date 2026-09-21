#include <QtTest>
#include <QTemporaryFile>
#include <cstring>
#include "onnx_engine/runtime.hpp"
using namespace onnx_engine;

class TestOnnxEngine : public QObject {
    Q_OBJECT
private:
    static TensorBuffer f32buf(const std::vector<int64_t>& shape, float v) {
        TensorBuffer b; b.type = ElementType::Float32; b.shape = shape;
        std::size_t n = 1; for (auto d : shape) n *= std::size_t(d);
        b.data.resize(n * sizeof(float));
        for (std::size_t i = 0; i < n; ++i) std::memcpy(b.data.data() + i * sizeof(float), &v, sizeof(float));
        return b;
    }

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

    void successClearsError() {
        std::string err = "stale";
        auto bad = Runtime::instance().session("/no/such/model.onnx", {}, err);
        QVERIFY(!bad);
        QVERIFY(!err.empty());

        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        auto ok = Runtime::instance().session(fx, {}, err);
        QVERIFY(ok);
        QVERIFY(err.empty());
    }

    void cacheHitReturnsSamePointer() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string e1, e2;
        auto a = Runtime::instance().session(fx, {}, e1);
        auto b = Runtime::instance().session(fx, {}, e2);
        QVERIFY(a);
        QVERIFY(b);
        QVERIFY(a.get() == b.get());
        QVERIFY(e1.empty());
        QVERIFY(e2.empty());
    }

    void corruptModelFails() {
        QTemporaryFile f;
        QVERIFY(f.open());
        QCOMPARE(f.write("not an onnx"), qint64(11));
        f.flush();

        std::string err;
        auto s = Runtime::instance().session(f.fileName().toStdString(), {}, err);
        QVERIFY(!s);
        QVERIFY(!err.empty());
    }

    void runAdd() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY2(s, err.c_str());
        std::vector<TensorBuffer> ins{f32buf({3,4}, 2.0f), f32buf({3,4}, 3.0f)}, outs;
        QVERIFY2(s->run(ins, outs, err), err.c_str());
        QCOMPARE(outs.size(), std::size_t(1));
        QCOMPARE(outs[0].type, ElementType::Float32);
        QCOMPARE(outs[0].shape, (std::vector<int64_t>{3,4}));
        QCOMPARE(outs[0].data.size(), std::size_t(12 * sizeof(float)));
        float got = 0; std::memcpy(&got, outs[0].data.data(), sizeof(float));
        QCOMPARE(got, 5.0f);
    }

    void runDynamicAdd() {
        const std::string dyn = std::string(ORTDRAW_TEST_DATA_DIR) + "/add_dynamic.onnx";
        std::string err;
        auto s = Runtime::instance().session(dyn, {}, err);
        QVERIFY2(s, err.c_str());
        std::vector<TensorBuffer> ins{f32buf({2,4}, 1.0f), f32buf({2,4}, 2.0f)}, outs;
        QVERIFY2(s->run(ins, outs, err), err.c_str());
        QCOMPARE(outs.size(), std::size_t(1));
        QCOMPARE(outs[0].shape, (std::vector<int64_t>{2,4}));
        QCOMPARE(outs[0].data.size(), std::size_t(8 * sizeof(float)));
    }

    void runWrongInputCountFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY(s);
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({f32buf({3,4}, 1.0f)}, outs, err));
        QVERIFY(!err.empty());
    }

    void runWrongShapeFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY(s);
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({f32buf({2,4}, 1.0f), f32buf({3,4}, 1.0f)}, outs, err));
        QVERIFY(!err.empty());
    }
};
QTEST_MAIN(TestOnnxEngine)
#include "test_onnx_engine.moc"
