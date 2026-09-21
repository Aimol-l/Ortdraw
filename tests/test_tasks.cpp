#include <QtTest>
#include <variant>
#include "engine/tasks/PreProcessRegistry.hpp"
#include "engine/tasks/PostProcessRegistry.hpp"
#include "engine/executors/PreProcessExecutor.hpp"
#include "engine/executors/PostProcessExecutor.hpp"
#include <opencv2/imgproc.hpp>

namespace {

QVector<NodeData> imageInputs(const cv::Mat& img) {
    QVector<NodeData> v;
    v.push_back(img);
    return v;
}

} // namespace

class TestTasks : public QObject {
    Q_OBJECT
private slots:
    void initTestCase() {
        registerBuiltinPreProcessTasks();
        registerBuiltinPostProcessTasks();
    }

    void registryHasPreTasks() {
        const auto& all = PreProcessRegistry::instance().all();
        QStringList ids;
        for (const auto& s : all) ids << s.id;
        QVERIFY(ids.contains("image_to_tensor"));
        QVERIFY(ids.contains("yolo_letterbox"));
    }

    void imageToTensorTask() {
        cv::Mat bgr(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "image_to_tensor"},
            {"params", QVariantMap{{"width", 2}, {"height", 2}, {"layout", "NCHW"},
                                   {"channel", "rgb"}, {"norm", "div255"}, {"resize", "keep"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<Tensor>(r.outputs[0]));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(t.shape().size(), size_t(4));
        QCOMPARE(int64_t(t.shape()[0]), int64_t(1));
        QCOMPARE(int64_t(t.shape()[1]), int64_t(3));
    }

    void letterboxTask() {
        cv::Mat gray(10, 20, CV_8U, cv::Scalar(128));
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "yolo_letterbox"},
            {"params", QVariantMap{{"size", 32}, {"pad", 114}, {"channel", "rgb"}}}},
            imageInputs(gray));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int64_t(t.shape()[2]), int64_t(32));
        QCOMPARE(int64_t(t.shape()[3]), int64_t(32));
    }

    void registryHasPostTasks() {
        const auto& all = PostProcessRegistry::instance().all();
        QStringList ids;
        for (const auto& s : all) ids << s.id;
        QVERIFY(ids.contains("yolo_detect"));
        QVERIFY(ids.contains("yolo_segment"));
        QVERIFY(ids.contains("classify"));
    }

    void yoloDetectSynthetic() {
        // [1,84,1]：类别 5 分数 0.9，框 cx=320,cy=320,w=100,h=100
        std::vector<float> v(84, 0.0f);
        v[0] = 320; v[1] = 320; v[2] = 100; v[3] = 100; v[4 + 5] = 0.9f;
        std::vector<int64_t> sh{1, 84, 1};
        Tensor t(v, sh);
        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_detect"},
            {"params", QVariantMap{{"networkSize", 640}, {"conf", 0.25}, {"iou", 0.45}}}},
            QVector<NodeData>{ t });
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<cv::Mat>(r.outputs[0]));
        QCOMPARE(std::get<cv::Mat>(r.outputs[0]).cols, 640);
    }

    void classifySyntheticReportsDisplay() {
        std::vector<float> v{0.1f, 0.2f, 0.7f, 0.05f, 0.05f};  // argmax = 2
        std::vector<int64_t> sh{1, 5};
        Tensor t(v, sh);
        QVariantMap captured;
        ExecuteContext ctx; ctx.display = [&](const QVariantMap& d) { captured = d; };
        PostProcessExecutor ex;
        const ExecResult r = ex.execute(ctx, QVariantMap{{"task", "classify"},
            {"params", QVariantMap{{"topk", 3}}}}, QVector<NodeData>{ t });
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<double>(r.outputs[0]));
        QCOMPARE(std::get<double>(r.outputs[0]), 2.0);
        QCOMPARE(captured.value("top1").toInt(), 2);
        QVERIFY(captured.contains("topk"));
    }
};

QTEST_MAIN(TestTasks)
#include "test_tasks.moc"
