#include <QtTest>
#include <QTemporaryDir>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "engine/BuiltinExecutors.hpp"
#include "engine/executors/BlurExecutor.hpp"
#include "engine/executors/ConvExecutor.hpp"
#include "engine/executors/EdgeDetectExecutor.hpp"
#include "engine/executors/GrayExecutor.hpp"
#include "engine/executors/ImageLoadExecutor.hpp"
#include "engine/executors/ImageSaveExecutor.hpp"
#include "engine/executors/ImageShowExecutor.hpp"
#include "engine/executors/ResizeExecutor.hpp"
#include "engine/executors/ThresholdExecutor.hpp"

namespace {

cv::Mat makeGray(int rows = 8, int cols = 8) {
    cv::Mat m(rows, cols, CV_8U);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            m.at<uchar>(r, c) = uchar(((r * cols + c) * 4) % 256);
    return m;
}

cv::Mat makeStepEdge(int rows = 16, int cols = 16) {
    cv::Mat m(rows, cols, CV_8U, cv::Scalar(0));
    m.colRange(cols / 2, cols).setTo(255);
    return m;
}

cv::Mat makeBGR(int rows = 8, int cols = 8) {
    cv::Mat m(rows, cols, CV_8UC3);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            m.at<cv::Vec3b>(r, c) = cv::Vec3b(uchar((r * 31 + c * 7) % 256),
                                              uchar((r * 17 + c * 53) % 256),
                                              uchar((r * 91 + c * 3) % 256));
    return m;
}

QVector<NodeData> imageInputs(const cv::Mat& img) {
    QVector<NodeData> v;
    v.push_back(img);
    return v;
}

Tensor identityKernel() {
    std::vector<float> k{0.f, 0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 0.f};
    std::vector<int64_t> s{3, 3};
    return Tensor(k, s);
}

} // namespace

class TestExecutors : public QObject {
    Q_OBJECT
private slots:
    void resizeBySize() {
        ResizeExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        QVariantMap p{{"mode", 0}, {"outWidth", 20}, {"outHeight", 10}};
        const ExecResult r = ex.execute({}, p, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 1);
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, 20);
        QCOMPARE(out.rows, 10);
    }

    void resizeByPercent() {
        ResizeExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        QVariantMap p{{"mode", 1}, {"percent", 50}};
        const ExecResult r = ex.execute({}, p, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, 4);
        QCOMPARE(out.rows, 4);
    }

    void blurKeepsSizeAndType() {
        BlurExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        const ExecResult r = ex.execute({}, QVariantMap{{"kernel", 3}}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, in.cols);
        QCOMPARE(out.rows, in.rows);
        QCOMPARE(out.type(), CV_8U);
    }

    void thresholdProducesBinary() {
        ThresholdExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        const ExecResult r = ex.execute({}, QVariantMap{{"threshold", 128}}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        for (int y = 0; y < out.rows; ++y)
            for (int x = 0; x < out.cols; ++x) {
                const uchar v = out.at<uchar>(y, x);
                QVERIFY(v == 0 || v == 255);
            }
    }

    void convIdentityPreservesImage() {
        ConvExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        QVector<NodeData> inputs = imageInputs(in);
        inputs.push_back(identityKernel());
        const ExecResult r = ex.execute({}, {}, inputs);
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, in.cols);
        QCOMPARE(out.rows, in.rows);
        cv::Mat diff;
        cv::absdiff(out, in, diff);
        QCOMPARE(cv::countNonZero(diff.reshape(1)), 0);
    }

    void convWithoutKernelFails() {
        ConvExecutor ex;
        const ExecResult r = ex.execute({}, {}, imageInputs(makeGray()));
        QVERIFY(!r.ok);
    }

    void grayFromColorProducesSingleChannel() {
        GrayExecutor ex;
        const cv::Mat in = makeBGR(8, 8);
        const ExecResult r = ex.execute({}, {}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 1);
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.channels(), 1);

        cv::Mat ref;
        cv::cvtColor(in, ref, cv::COLOR_BGR2GRAY);
        cv::Mat diff;
        cv::absdiff(out, ref, diff);
        QCOMPARE(cv::countNonZero(diff.reshape(1)), 0);
    }

    void graySingleChannelPassthrough() {
        GrayExecutor ex;
        const cv::Mat in = makeGray(8, 6);
        const ExecResult r = ex.execute({}, {}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.channels(), 1);
        QCOMPARE(out.cols, in.cols);
        QCOMPARE(out.rows, in.rows);
    }

    void grayThenThresholdChain() {
        GrayExecutor gray;
        const cv::Mat in = makeBGR(8, 8);
        const ExecResult g = gray.execute({}, {}, imageInputs(in));
        QVERIFY2(g.ok, qPrintable(g.error));

        ThresholdExecutor thr;
        const ExecResult r = thr.execute({}, QVariantMap{{"threshold", 128}}, g.outputs);
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.channels(), 1);
    }

    void imageLoadEmptyPathFails() {
        ImageLoadExecutor ex;
        QVERIFY(!ex.execute({}, QVariantMap{{"path", ""}}, {}).ok);
        QVERIFY(!ex.execute({}, QVariantMap{}, {}).ok);
        QVERIFY(!ex.execute({}, QVariantMap{{"path", "/no/such/file.png"}}, {}).ok);
    }

    void imageLoadRoundTrip() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("img.png");
        QVERIFY(cv::imwrite(path.toStdString(), makeGray(6, 5)));

        ImageLoadExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"path", path}}, {});
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QVERIFY(!out.empty());
        QCOMPARE(out.cols, 5);
        QCOMPARE(out.rows, 6);
    }

    void imageShowPassthrough() {
        ImageShowExecutor ex;
        const cv::Mat in = makeGray(8, 8);
        const ExecResult r = ex.execute({}, {}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 1);
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, in.cols);
        QCOMPARE(out.rows, in.rows);
    }

    void imageSaveWritesFileAndPassthrough() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("out.png");

        ImageSaveExecutor ex;
        const cv::Mat in = makeGray(8, 6);
        const ExecResult r = ex.execute({}, QVariantMap{{"path", path}}, imageInputs(in));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 1);
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, in.cols);
        QCOMPARE(out.rows, in.rows);

        const QFileInfo fi(path);
        QVERIFY(fi.exists());
        QVERIFY(fi.size() > 0);
    }

    void imageSaveAppendsNameToDirectory() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());

        ImageSaveExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"path", dir.path()}}, imageInputs(makeGray()));
        QVERIFY2(r.ok, qPrintable(r.error));

        const QDir d(dir.path());
        const QStringList files = d.entryList(QDir::Files);
        QCOMPARE(files.size(), 1);
        QVERIFY(QFileInfo(d.filePath(files.first())).size() > 0);
    }

    void imageSaveWithoutInputFails() {
        ImageSaveExecutor ex;
        QVERIFY(!ex.execute({}, QVariantMap{{"path", "/tmp/x.png"}}, {}).ok);
    }

    void edgeDetectProducesEdges() {
        EdgeDetectExecutor ex;
        const cv::Mat in = makeStepEdge(16, 16);
        for (int method = 0; method <= 3; ++method) {
            QVariantMap p{{"method", method}, {"kernel", 3}, {"low", 50}, {"high", 150}};
            const ExecResult r = ex.execute({}, p, imageInputs(in));
            QVERIFY2(r.ok, qPrintable(r.error));
            QCOMPARE(r.outputs.size(), 1);
            const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
            QVERIFY(!out.empty());
            QCOMPARE(out.channels(), 1);
            QCOMPARE(out.type(), CV_8U);
            QVERIFY2(cv::countNonZero(out) > 0,
                     qPrintable(QString("method %1 produced no edges").arg(method)));
        }
    }

    void cannyLowerThresholdDetectsAtLeastAsMany() {
        EdgeDetectExecutor ex;
        const cv::Mat in = makeStepEdge(32, 32);
        const ExecResult rl = ex.execute({}, QVariantMap{{"method", 3}, {"kernel", 3}, {"low", 50}, {"high", 100}}, imageInputs(in));
        const ExecResult rh = ex.execute({}, QVariantMap{{"method", 3}, {"kernel", 3}, {"low", 200}, {"high", 250}}, imageInputs(in));
        QVERIFY2(rl.ok, qPrintable(rl.error));
        QVERIFY2(rh.ok, qPrintable(rh.error));
        const cv::Mat& lo = std::get<cv::Mat>(rl.outputs[0]);
        const cv::Mat& hi = std::get<cv::Mat>(rh.outputs[0]);
        QCOMPARE(lo.type(), CV_8U);
        QCOMPARE(hi.type(), CV_8U);
        QVERIFY(cv::countNonZero(lo) >= cv::countNonZero(hi));
    }

    void registrationCoversBuiltins() {
        registerBuiltinExecutors();
        const QStringList types = NodeRegistry::instance().knownTypes();
        for (const char* t : {"ImageLoad", "ImageShow", "ImageSave", "Resize", "Blur", "Threshold", "Gray", "EdgeDetect", "Conv"})
            QVERIFY(types.contains(t));
    }
};

QTEST_MAIN(TestExecutors)
#include "test_executors.moc"
