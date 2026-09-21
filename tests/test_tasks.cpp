#include <QtTest>
#include <variant>
#include "engine/tasks/PreProcessRegistry.hpp"
#include "engine/executors/PreProcessExecutor.hpp"
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
    void initTestCase() { registerBuiltinPreProcessTasks(); }

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
};

QTEST_MAIN(TestTasks)
#include "test_tasks.moc"
