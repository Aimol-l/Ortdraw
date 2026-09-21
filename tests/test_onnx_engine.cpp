#include <QtTest>
#include <QTemporaryFile>
#include <cstring>
#include <thread>
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

    void runWrongDtypeFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY(s);
        TensorBuffer i32;
        i32.type = ElementType::Int32;
        i32.shape = {3,4};
        i32.data.resize(12 * sizeof(std::int32_t));
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({i32, i32}, outs, err));
        QVERIFY(!err.empty());
    }

    void runWrongRankFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY(s);
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({f32buf({3,4}, 1.0f), f32buf({3,4,1}, 1.0f)}, outs, err));
        QVERIFY(!err.empty());
    }

    void runWrongDataSizeFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY(s);
        TensorBuffer truncated = f32buf({3,4}, 1.0f);
        truncated.data.resize(truncated.data.size() - sizeof(float));
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({truncated, f32buf({3,4}, 2.0f)}, outs, err));
        QVERIFY(!err.empty());
    }

    void runRejectsNonTensorOutput() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/seq_out.onnx";
        std::string err;
        auto s = Runtime::instance().session(fx, {}, err);
        QVERIFY2(s, err.c_str());
        QCOMPARE(s->info().outputs.size(), std::size_t(1));
        QVERIFY(!s->info().outputs[0].isTensor);
        std::vector<TensorBuffer> ins{f32buf({2}, 1.0f)}, outs;
        QVERIFY(!s->run(ins, outs, err));
        QVERIFY(!err.empty());
    }

    void cpuDeviceRunsAdd() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 1;
        auto s = Runtime::instance().session(fx, o, err);
        QVERIFY2(s, err.c_str());
        std::vector<TensorBuffer> ins{f32buf({3,4}, 1.0f), f32buf({3,4}, 2.0f)}, outs;
        QVERIFY2(s->run(ins, outs, err), err.c_str());
        QCOMPARE(outs.size(), std::size_t(1));
    }

    void autoDeviceFallsBackOrRuns() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        SessionOptions o; o.device = Device::Auto;
        auto s = Runtime::instance().session(fx, o, err);
        QVERIFY2(s, err.c_str());          // CUDA 不可用时回退 CPU 并成功
        std::vector<TensorBuffer> ins{f32buf({3,4}, 1.0f), f32buf({3,4}, 1.0f)}, outs;
        QVERIFY2(s->run(ins, outs, err), err.c_str());
    }

    void cudaProbeDoesNotCrash() {
        (void)Runtime::cudaAvailable();    // 仅要求不崩溃
    }

    void sameKeySharesSession() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string e1, e2;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 2;
        auto a = Runtime::instance().session(fx, o, e1);
        auto b = Runtime::instance().session(fx, o, e2);
        QVERIFY(a && b);
        QVERIFY(a.get() == b.get());
    }

    void differentThreadsDifferentSession() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string e1, e2;
        SessionOptions o1; o1.device = Device::CPU; o1.intraThreads = 1;
        SessionOptions o2; o2.device = Device::CPU; o2.intraThreads = 3;
        auto a = Runtime::instance().session(fx, o1, e1);
        auto b = Runtime::instance().session(fx, o2, e2);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());
    }

    void clearCacheReloads() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 5;
        auto a = Runtime::instance().session(fx, o, err);
        Runtime::instance().clearCache();
        auto b = Runtime::instance().session(fx, o, err);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());
    }

    void reloadInvalidates() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 6;
        auto a = Runtime::instance().session(fx, o, err);
        Runtime::instance().reload(fx);
        auto b = Runtime::instance().session(fx, o, err);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());
    }

    // LRU：限制缓存为 1 后，仅由缓存持有（use_count==1）的旧条目应被淘汰。
    // 正在被外部 shared_ptr 持有的条目不会被淘汰，故先 reset(a) 再触发 b 的插入，
    // 使淘汰时机确定、测试稳定。
    void cacheLimitEvicts() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        Runtime::instance().clearCache();
        Runtime::instance().setCacheLimits(1, std::size_t(1) << 30);
        std::string e1, e2;
        SessionOptions o1; o1.device = Device::CPU; o1.intraThreads = 11;
        SessionOptions o2; o2.device = Device::CPU; o2.intraThreads = 12;
        auto a = Runtime::instance().session(fx, o1, e1);
        QVERIFY2(a, e1.c_str());
        std::weak_ptr<Session> wa = a;
        a.reset();                                       // 释放外部引用，仅缓存持有
        auto b = Runtime::instance().session(fx, o2, e2); // 触发淘汰
        QVERIFY2(b, e2.c_str());
        QVERIFY(wa.expired());                           // 旧会话已淘汰
        Runtime::instance().setCacheLimits(4, std::size_t(1) << 30);
    }

    void concurrentSameKeyReturnsOneSession() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        SessionOptions opts; opts.device = Device::CPU; opts.intraThreads = 2;
        constexpr int N = 8;
        std::vector<std::shared_ptr<Session>> got(N);
        std::vector<std::string> errs(N);
        std::vector<std::thread> ts;
        ts.reserve(N);
        for (int i = 0; i < N; ++i) {
            ts.emplace_back([&, i] {
                got[i] = Runtime::instance().session(fx, opts, errs[i]);
            });
        }
        for (auto& t : ts) t.join();
        for (int i = 0; i < N; ++i) {
            QVERIFY2(got[i], errs[i].c_str());
            QCOMPARE(got[i].get(), got[0].get());
        }
    }

    void explicitCudaWithoutProviderFails() {
        const std::string fx = std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
        SessionOptions o; o.device = Device::CUDA;
        std::string err;
        auto s = Runtime::instance().session(fx, o, err);
        if (!s) {
            // 无可用 CUDA：必须报错而非静默回退
            QVERIFY(!err.empty());
        } else {
            std::vector<TensorBuffer> ins{f32buf({3,4}, 1.0f), f32buf({3,4}, 2.0f)}, outs;
            QVERIFY2(s->run(ins, outs, err), err.c_str());
        }
    }
};
QTEST_MAIN(TestOnnxEngine)
#include "test_onnx_engine.moc"
