#include <QtTest>
#include "engine/onnx/OnnxTensorConvert.hpp"
#include <opencv2/imgproc.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

class TestOnnxConvert : public QObject {
    Q_OBJECT
private:
    static std::vector<int64_t> shapeOf(const Tensor& t) {
        const auto s = t.shape();
        return std::vector<int64_t>(s.begin(), s.end());
    }

private slots:
    void dtypeMappingRoundTrip() {
        QCOMPARE(onnx_engine::toViaDataType(onnx_engine::ElementType::Float32), via::DataType::FLOAT32);
        QCOMPARE(onnx_engine::toViaDataType(onnx_engine::ElementType::Int64),   via::DataType::INT64);
        QVERIFY(!onnx_convert::isSupported(onnx_engine::ElementType::UInt8));   // UInt8 不受支持
        QCOMPARE(onnx_engine::toViaDataType(onnx_engine::ElementType::Bool),    via::DataType::INT8);   // bool->int8
        QCOMPARE(onnx_engine::fromViaDataType(via::DataType::FLOAT32), onnx_engine::ElementType::Float32);
        QCOMPARE(onnx_engine::fromViaDataType(via::DataType::INT16),   onnx_engine::ElementType::Int16);
    }

    void imageToTensorNchwDiv255() {
        cv::Mat bgr(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));   // BGR 全红
        const Tensor t = onnx_convert::imageToTensor(bgr, {1, 3, 2, 2},
                                                     onnx_engine::ElementType::Float32,
                                                     "div255", {}, {}, "rgb", "keep");
        QCOMPARE(shapeOf(t), (std::vector<int64_t>{1, 3, 2, 2}));
        QCOMPARE(t.dtype(), via::DataType::FLOAT32);
        const float* p = reinterpret_cast<const float*>(t.data());
        QCOMPARE(p[0], 1.0f);   // RGB 的 R 通道 = 255/255 = 1
    }

    void imageToTensorNhwc() {
        cv::Mat gray(2, 2, CV_8U, cv::Scalar(128));
        const Tensor t = onnx_convert::imageToTensor(gray, {1, 2, 2, 1},
                                                     onnx_engine::ElementType::Float32,
                                                     "div255", {}, {}, "auto", "keep");
        QCOMPARE(shapeOf(t), (std::vector<int64_t>{1, 2, 2, 1}));
        const float* p = reinterpret_cast<const float*>(t.data());
        QVERIFY(qAbs(p[0] - 128.0f/255.0f) < 1e-6f);
    }

    void tensorToNodeDataFloatAndScalars() {
        std::vector<float> vals{1, 2, 3, 4};
        Tensor t(vals, std::vector<int64_t>{2, 2});
        QVERIFY(std::holds_alternative<Tensor>(onnx_convert::tensorToNodeData(t)));

        std::vector<std::int8_t> bv{1};                 // 声明 Bool → bool
        Tensor bt(bv, std::vector<int64_t>{1});
        QVERIFY(std::holds_alternative<bool>(
            onnx_convert::tensorToNodeData(bt, onnx_engine::ElementType::Bool)));

        std::vector<std::int64_t> iv{7};                // 其它标量 → double
        Tensor it(iv, std::vector<int64_t>{1});
        const NodeData d = onnx_convert::tensorToNodeData(it);
        QVERIFY(std::holds_alternative<double>(d));
        QCOMPARE(std::get<double>(d), 7.0);
    }

    void tensorRoundTrip() {
        std::vector<float> v{1,2,3,4}; std::vector<int64_t> sh{2,2};
        Tensor t(v, sh);
        QCOMPARE(shapeOf(t), (std::vector<int64_t>{2,2}));
        QCOMPARE(t.dtype(), via::DataType::FLOAT32);
        QCOMPARE(t.numel(), std::size_t(4));
        QCOMPARE(onnx_convert::tensorElement(t, 0), 1.0f);
    }

    void imageToTensorKeepSizeMismatchReturnsEmpty() {
        cv::Mat bgr(2, 3, CV_8UC3, cv::Scalar(0, 0, 255));
        const Tensor t = onnx_convert::imageToTensor(bgr, {1, 3, 100, 7},
                                                     onnx_engine::ElementType::Float32,
                                                     "div255", {}, {}, "rgb", "keep");
        QCOMPARE(t.numel(), std::size_t(0));
    }

    void imageToTensorGrayNchw() {
        cv::Mat gray(2, 3, CV_8U, cv::Scalar(128));
        const Tensor t = onnx_convert::imageToTensor(gray, {1, 1, 2, 3},
                                                     onnx_engine::ElementType::Float32,
                                                     "div255", {}, {}, "auto", "keep");
        QCOMPARE(shapeOf(t), (std::vector<int64_t>{1, 1, 2, 3}));
        const float* p = reinterpret_cast<const float*>(t.data());
        QVERIFY(qAbs(p[0] - 128.0f/255.0f) < 1e-6f);
    }

    void imageToTensorChannelMismatchReturnsEmpty() {
        cv::Mat gray(2, 3, CV_8U, cv::Scalar(128));
        const Tensor t = onnx_convert::imageToTensor(gray, {1, 3, 2, 3},
                                                     onnx_engine::ElementType::Float32,
                                                     "div255", {}, {}, "auto", "keep");
        QCOMPARE(t.numel(), std::size_t(0));
    }

    void rank0FloatYieldsNumber() {
        std::vector<float> v{3.5f};                     // numel==1 标量承载
        Tensor t(v, std::vector<int64_t>{1});
        const NodeData d = onnx_convert::tensorToNodeData(t);
        QVERIFY(std::holds_alternative<double>(d));
        QCOMPARE(std::get<double>(d), 3.5);
    }

    void singleElementShape1IntYieldsNumber() {
        std::vector<std::int64_t> v{7};
        Tensor t(v, std::vector<int64_t>{1});
        const NodeData d = onnx_convert::tensorToNodeData(t);
        QVERIFY(std::holds_alternative<double>(d));
        QCOMPARE(std::get<double>(d), 7.0);
    }

    void emptyTensorYieldsMonostate() {
        Tensor t;   // 默认构造：numel==0
        QVERIFY(std::holds_alternative<std::monostate>(onnx_convert::tensorToNodeData(t)));
    }

    void declaredTypeDisambiguatesBoolAndInt8Scalars() {
        std::vector<std::int8_t> v{1};
        Tensor t(v, std::vector<int64_t>{1});
        // 声明为 Bool → bool
        const NodeData b = onnx_convert::tensorToNodeData(t, onnx_engine::ElementType::Bool);
        QVERIFY(std::holds_alternative<bool>(b));
        QCOMPARE(std::get<bool>(b), true);
        // 声明为 Int8 → double
        const NodeData i = onnx_convert::tensorToNodeData(t, onnx_engine::ElementType::Int8);
        QVERIFY(std::holds_alternative<double>(i));
        QCOMPARE(std::get<double>(i), 1.0);
        // 1 参退化（Unknown）：INT8 标量 → double，不退化为 bool
        const NodeData u = onnx_convert::tensorToNodeData(t);
        QVERIFY(std::holds_alternative<double>(u));
        QCOMPARE(std::get<double>(u), 1.0);
        // 非标量即使声明为 Bool 也保持张量
        std::vector<std::int8_t> vv{1, 0};
        Tensor tt(vv, std::vector<int64_t>{2});
        QVERIFY(std::holds_alternative<Tensor>(
            onnx_convert::tensorToNodeData(tt, onnx_engine::ElementType::Bool)));
    }
};

QTEST_MAIN(TestOnnxConvert)
#include "test_onnx_convert.moc"
