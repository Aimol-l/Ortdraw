#include <QtTest>
#include "engine/onnx/OnnxTensorConvert.hpp"
#include <opencv2/imgproc.hpp>

class TestOnnxConvert : public QObject {
    Q_OBJECT
private slots:
    void dtypeMappingRoundTrip() {
        using namespace onnx_convert;
        QCOMPARE(toViaType(onnx_engine::ElementType::Float32), via::DataType::FLOAT32);
        QCOMPARE(toViaType(onnx_engine::ElementType::Int64),   via::DataType::INT64);
        QCOMPARE(toViaType(onnx_engine::ElementType::UInt8),   via::DataType::INT16);  // 无符号->有符号16
        QCOMPARE(toViaType(onnx_engine::ElementType::Bool),    via::DataType::INT8);   // bool->int8
        QCOMPARE(fromViaType(via::DataType::FLOAT32), onnx_engine::ElementType::Float32);
        QCOMPARE(fromViaType(via::DataType::INT16),   onnx_engine::ElementType::Int16);
    }

    void imageToTensorNchwDiv255() {
        cv::Mat bgr(2, 3, CV_8UC3, cv::Scalar(0, 0, 255));   // BGR 全红
        const auto t = onnx_convert::imageToTensor(bgr, {1, 3, 2, 3},
                                                   onnx_engine::ElementType::Float32,
                                                   "div255", {}, {}, "rgb", "keep");
        QCOMPARE(t.shape, (std::vector<int64_t>{1, 3, 2, 3}));
        QCOMPARE(t.type, onnx_engine::ElementType::Float32);
        const float* p = reinterpret_cast<const float*>(t.data.data());
        QCOMPARE(p[0], 1.0f);   // RGB 的 R 通道 = 255/255 = 1
    }

    void imageToTensorNhwc() {
        cv::Mat gray(2, 2, CV_8U, cv::Scalar(128));
        const auto t = onnx_convert::imageToTensor(gray, {1, 2, 2, 1},
                                                   onnx_engine::ElementType::Float32,
                                                   "div255", {}, {}, "auto", "keep");
        QCOMPARE(t.shape, (std::vector<int64_t>{1, 2, 2, 1}));
        const float* p = reinterpret_cast<const float*>(t.data.data());
        QVERIFY(qAbs(p[0] - 128.0f/255.0f) < 1e-6f);
    }

    void tensorToNodeDataFloatAndScalars() {
        onnx_engine::TensorBuffer b; b.type = onnx_engine::ElementType::Float32;
        b.shape = {2, 2}; b.data.resize(4 * 4);
        float vals[4] = {1, 2, 3, 4}; std::memcpy(b.data.data(), vals, sizeof(vals));
        QVERIFY(std::holds_alternative<Tensor>(onnx_convert::tensorToNodeData(b)));

        onnx_engine::TensorBuffer bb; bb.type = onnx_engine::ElementType::Bool;
        bb.shape = {}; bb.data = {1};
        QVERIFY(std::holds_alternative<bool>(onnx_convert::tensorToNodeData(bb)));

        onnx_engine::TensorBuffer bi; bi.type = onnx_engine::ElementType::Int64;
        bi.shape = {}; bi.data.resize(8); int64_t v = 7; std::memcpy(bi.data.data(), &v, 8);
        const NodeData d = onnx_convert::tensorToNodeData(bi);
        QVERIFY(std::holds_alternative<double>(d));
        QCOMPARE(std::get<double>(d), 7.0);
    }

    void tensorRoundTrip() {
        std::vector<float> v{1,2,3,4}; std::vector<int64_t> sh{2,2};
        Tensor t(v, sh);
        const auto b = onnx_convert::tensorToBuffer(t);
        QCOMPARE(b.shape, (std::vector<int64_t>{2,2}));
        QCOMPARE(b.type, onnx_engine::ElementType::Float32);
        QCOMPARE(b.data.size(), std::size_t(4 * sizeof(float)));
    }

    void imageToTensorKeepSizeMismatchReturnsEmpty() {
        cv::Mat bgr(2, 3, CV_8UC3, cv::Scalar(0, 0, 255));
        const auto t = onnx_convert::imageToTensor(bgr, {1, 3, 100, 3},
                                                   onnx_engine::ElementType::Float32,
                                                   "div255", {}, {}, "rgb", "keep");
        QVERIFY(t.data.empty());
    }

    void unsupportedTypeYieldsMonostate() {
        onnx_engine::TensorBuffer b; b.type = onnx_engine::ElementType::UInt16;
        b.shape = {2}; b.data.resize(4);
        QVERIFY(std::holds_alternative<std::monostate>(onnx_convert::tensorToNodeData(b)));
    }
};

QTEST_MAIN(TestOnnxConvert)
#include "test_onnx_convert.moc"
