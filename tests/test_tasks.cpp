#include <QtTest>
#include <variant>
#include "engine/tasks/PreProcessRegistry.hpp"
#include "node/PreProcess.hpp"
#include "node/PostProcess.hpp"
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
        QCOMPARE(ids.size(), 2);                       // 只保留两个任务
        QVERIFY(ids.contains("standard"));
        QVERIFY(ids.contains("yolo_letterbox"));
        QVERIFY(!ids.contains("normalize"));
        QVERIFY(!ids.contains("resize"));
    }

    void taskNodesHaveMinWidth() {
        PreProcessNode pre;
        PostProcessNode post;
        QVERIFY(pre.getMinWidth() >= 260);    // 保证参数行不越界
        QVERIFY(post.getMinWidth() >= 260);
    }

    void standardTaskFp32Range01() {
        cv::Mat bgr(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));   // BGR 全红
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "resize"}, {"sizeWH", "2x2"},
                                   {"layout", "NCHW"}, {"channel", "rgb"}, {"dtype", "fp32"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<Tensor>(r.outputs[0]));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int64_t(t.shape()[0]), int64_t(1));
        QCOMPARE(int64_t(t.shape()[1]), int64_t(3));
        QCOMPARE(int64_t(t.shape()[2]), int64_t(2));
        QCOMPARE(int64_t(t.shape()[3]), int64_t(2));
        // 值域 0..1（红色 R 通道 = 1.0）
        const float* p = reinterpret_cast<const float*>(t.data());
        QCOMPARE(p[0], 1.0f);
    }

    void standardTaskFp16() {
        cv::Mat bgr(2, 2, CV_8UC3, cv::Scalar(0, 0, 255));
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "resize"}, {"sizeWH", "2x2"},
                                   {"layout", "NCHW"}, {"channel", "rgb"}, {"dtype", "fp16"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int(t.dtype()), int(via::DataType::FLOAT16));
        QCOMPARE(t.data() != nullptr, true);
    }

    void standardTaskZscore() {
        cv::Mat bgr(1, 1, CV_8UC3, cv::Scalar(0, 0, 255));   // 红
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "keep"}, {"layout", "NCHW"}, {"channel", "rgb"},
                                   {"dtype", "fp32"}, {"norm", "zscore"},
                                   {"mean", "255,0,0"}, {"std", "255,1,1"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        const float* v = reinterpret_cast<const float*>(t.data());
        QCOMPARE(v[0], 0.0f);    // (255-255)/255
    }

    void standardTaskPm1() {
        cv::Mat bgr(1, 1, CV_8UC3, cv::Scalar(0, 0, 255));   // 红 → R=+1
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "keep"}, {"layout", "NCHW"}, {"channel", "rgb"},
                                   {"dtype", "fp32"}, {"norm", "pm1"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        const float* v = reinterpret_cast<const float*>(t.data());
        QCOMPARE(v[0], 1.0f);     // 255/127.5 - 1
        QCOMPARE(v[1], -1.0f);    // 0/127.5 - 1
    }

    void standardTaskMinMax() {
        cv::Mat bgr(1, 2, CV_8UC3);
        bgr.at<cv::Vec3b>(0, 0) = cv::Vec3b(0, 0, 0);
        bgr.at<cv::Vec3b>(0, 1) = cv::Vec3b(0, 0, 255);
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "keep"}, {"layout", "NCHW"}, {"channel", "rgb"},
                                   {"dtype", "fp32"}, {"norm", "minmax"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        const float* v = reinterpret_cast<const float*>(t.data());
        QCOMPARE(v[0], 0.0f);     // R 通道 min → 0
        QCOMPARE(v[1], 1.0f);     // R 通道 max → 1
    }

    void standardTaskKeepSize() {
        cv::Mat bgr(3, 5, CV_8UC3, cv::Scalar(10, 20, 30));
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "keep"}, {"layout", "NCHW"},
                                   {"channel", "rgb"}, {"dtype", "fp32"}}}},
            imageInputs(bgr));
        QVERIFY2(r.ok, qPrintable(r.error));
        const Tensor& t = std::get<Tensor>(r.outputs[0]);
        QCOMPARE(int64_t(t.shape()[2]), int64_t(3));   // H = 原图高
        QCOMPARE(int64_t(t.shape()[3]), int64_t(5));   // W = 原图宽
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
