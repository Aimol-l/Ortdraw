#include <QtTest>
#include <QFileInfo>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include "engine/tasks/PreProcessRegistry.hpp"
#include "engine/tasks/PostProcessRegistry.hpp"
#include "engine/executors/OnnxInferExecutor.hpp"

// 端到端：图像 -> 预处理(letterbox) -> ONNX(yolo11n) -> 后处理(检测)
// 依赖 gitignored 的 models/yolo11n.onnx 与一张测试图；缺失则跳过。
class TestOnnxE2E : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        registerBuiltinPreProcessTasks();
        registerBuiltinPostProcessTasks();
    }

    void yolo11nDetectionPipeline() {
        const QString model = QStringLiteral(ORTDRAW_MODELS_DIR) + "/yolo11n.onnx";
        if (!QFileInfo::exists(model)) QSKIP("models/yolo11n.onnx 不存在，跳过端到端验证");
        // 用一张能构造出来的测试图，避免依赖用户目录
        cv::Mat img(480, 640, CV_8UC3, cv::Scalar(200, 180, 160));
        cv::circle(img, {320, 240}, 80, cv::Scalar(60, 60, 220), cv::FILLED);

        QVector<NodeData> in; in.push_back(img);
        auto* pre = PreProcessRegistry::instance().find("yolo_letterbox");
        QVERIFY(pre);
        const ExecResult r1 = pre->compute({}, QVariantMap{{"size",640},{"pad",114},{"channel","rgb"}}, in);
        QVERIFY2(r1.ok, qPrintable(r1.error));
        QCOMPARE(r1.outputs.size(), 2);                          // 张量 + 元信息
        QVERIFY(std::holds_alternative<Tensor>(r1.outputs[0]));
        QVERIFY(std::holds_alternative<Tensor>(r1.outputs[1]));

        // 只把张量（index 0）喂 ONNX
        QVector<NodeData> onnxIn; onnxIn.push_back(r1.outputs[0]);
        OnnxInferExecutor onnx;
        const ExecResult r2 = onnx.execute({}, QVariantMap{{"modelPath", model}, {"device","cpu"}}, onnxIn);
        QVERIFY2(r2.ok, qPrintable(r2.error));
        QVERIFY(std::holds_alternative<Tensor>(r2.outputs[0]));
        const Tensor& o = std::get<Tensor>(r2.outputs[0]);
        QCOMPARE(int64_t(o.shape()[0]), int64_t(1));

        // 后处理：检测张量 + 原图 + 元信息
        QVector<NodeData> postIn;
        postIn.push_back(r2.outputs[0]);
        postIn.push_back(img);
        postIn.push_back(r1.outputs[1]);
        auto* post = PostProcessRegistry::instance().find("yolo_detect");
        QVERIFY(post);
        const ExecResult r3 = post->compute({}, QVariantMap{{"conf",0.25}}, postIn);
        QVERIFY2(r3.ok, qPrintable(r3.error));
        QVERIFY(std::holds_alternative<cv::Mat>(r3.outputs[0]));
        const cv::Mat& out = std::get<cv::Mat>(r3.outputs[0]);
        QCOMPARE(out.cols, img.cols);   // 输出为原图尺寸
        QCOMPARE(out.rows, img.rows);
    }
};

QTEST_MAIN(TestOnnxE2E)
#include "test_onnx_e2e.moc"
