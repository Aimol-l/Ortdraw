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

    void sizeParamZeroesWH() {
        PreProcessNode n;                                   // 默认 standard / 原尺寸
        QCOMPARE(n.taskParams().value("sizeWH").toString(), QString("0x0"));
        n.setTaskParam("size", "resize");                   // 切到“指定” → 给默认 224x224
        QCOMPARE(n.taskParams().value("sizeWH").toString(), QString("224x224"));
        n.setTaskParam("size", "keep");                     // 切回“原尺寸” → 归零
        QCOMPARE(n.taskParams().value("sizeWH").toString(), QString("0x0"));
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

    void preMetaForLetterbox() {
        cv::Mat img(480, 640, CV_8UC3, cv::Scalar(0, 0, 0));   // rows=480, cols=640
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "yolo_letterbox"},
            {"params", QVariantMap{{"size", 320}, {"pad", 114}}}},
            imageInputs(img));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 2);                          // 张量 + 元信息
        QVERIFY(std::holds_alternative<Tensor>(r.outputs[1]));
        const Tensor& m = std::get<Tensor>(r.outputs[1]);
        QCOMPARE(int64_t(m.numel()), int64_t(8));
        const float* v = reinterpret_cast<const float*>(m.data());
        QCOMPARE(v[0], 1.0f);      // mode = letterbox
        QCOMPARE(v[1], 640.0f);    // origW
        QCOMPARE(v[2], 480.0f);    // origH
        QCOMPARE(v[3], 320.0f);    // netW
        QCOMPARE(v[4], 320.0f);    // netH
        QCOMPARE(v[5], 0.5f);      // scale = 320/640
        QCOMPARE(v[6], 0.0f);      // padX
        QCOMPARE(v[7], 40.0f);     // padY = (320-240)/2
    }

    void preMetaForStandardKeep() {
        cv::Mat img(7, 9, CV_8UC3, cv::Scalar(10, 20, 30));     // rows=7, cols=9
        PreProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{
            {"task", "standard"},
            {"params", QVariantMap{{"size", "keep"}, {"layout", "NCHW"},
                                   {"channel", "rgb"}, {"dtype", "fp32"}}}},
            imageInputs(img));
        QVERIFY2(r.ok, qPrintable(r.error));
        QCOMPARE(r.outputs.size(), 2);
        const Tensor& m = std::get<Tensor>(r.outputs[1]);
        const float* v = reinterpret_cast<const float*>(m.data());
        QCOMPARE(v[0], 0.0f);      // mode = resize
        QCOMPARE(v[1], 9.0f);      // origW
        QCOMPARE(v[2], 7.0f);      // origH
        QCOMPARE(v[3], 9.0f);      // netW = 原尺寸 → 恒等
        QCOMPARE(v[4], 7.0f);      // netH
        QCOMPARE(v[5], 1.0f);
        QCOMPARE(v[6], 0.0f);
        QCOMPARE(v[7], 0.0f);
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
        // [1,84,1]：类别 5 分数 0.9，框 cx=160,cy=120,w=100,h=100（网络=原图恒等）
        std::vector<float> v(84, 0.0f);
        v[0] = 160; v[1] = 120; v[2] = 100; v[3] = 100; v[4 + 5] = 0.9f;
        Tensor detect(v, std::vector<int64_t>{1, 84, 1});
        cv::Mat original(240, 320, CV_8UC3, cv::Scalar(20, 20, 20));

        // mode=0 恒等元信息
        std::vector<float> mv{0, 320, 240, 320, 240, 1, 0, 0};
        Tensor meta(mv, std::vector<int64_t>{1, 8});

        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_detect"},
            {"params", QVariantMap{{"conf", 0.25}, {"iou", 0.45}}}},
            QVector<NodeData>{ detect, original, meta });
        QVERIFY2(r.ok, qPrintable(r.error));
        QVERIFY(std::holds_alternative<cv::Mat>(r.outputs[0]));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, 320);   // 输出为原图尺寸，不再是网络尺寸
        QCOMPARE(out.rows, 240);
    }

    void nmsIsPerClassAware() {
        // 直接测试解码+NMS：两个完全重叠的框
        auto makeHead = [](bool secondIsAnotherClass) {
            std::vector<float> v(2 * 84, 0.0f);
            for (int n = 0; n < 2; ++n) {
                v[n * 84 + 0] = 100; v[n * 84 + 1] = 100;
                v[n * 84 + 2] = 60;  v[n * 84 + 3] = 60;
            }
            v[0 * 84 + 4] = 0.90f;                              // 框0：类别 0
            v[1 * 84 + (secondIsAnotherClass ? 5 : 4)] = 0.80f; // 框1：类别 1 或 0
            return Tensor(v, std::vector<int64_t>{1, 2, 84});
        };
        std::vector<postprocess_detail::Detection> dets;
        // 同类重叠 → 抑制为一个
        QVERIFY(postprocess_detail::decodeDetections(makeHead(false), 0.25f, 0.45f, 300, dets));
        QCOMPARE(dets.size(), std::size_t(1));
        // 异类重叠 → 都保留（类内 NMS）
        QVERIFY(postprocess_detail::decodeDetections(makeHead(true), 0.25f, 0.45f, 300, dets));
        QCOMPARE(dets.size(), std::size_t(2));
    }

    void yoloDetectChannelsLastLayout() {
        // 同一检测结果，但检测头为 [1, M, 4+n]（channels-last，M=1）
        std::vector<float> v(84, 0.0f);
        v[0] = 160; v[1] = 120; v[2] = 100; v[3] = 100; v[4 + 5] = 0.9f;
        Tensor detect(v, std::vector<int64_t>{1, 1, 84});
        cv::Mat original(240, 320, CV_8UC3, cv::Scalar(20, 20, 20));
        std::vector<float> mv{0, 320, 240, 320, 240, 1, 0, 0};
        Tensor meta(mv, std::vector<int64_t>{1, 8});
        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_detect"},
            {"params", QVariantMap{{"conf", 0.25}, {"iou", 0.45}}}},
            QVector<NodeData>{ detect, original, meta });
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, 320);
        QCOMPARE(out.rows, 240);
    }

    void yoloDetectMetaSizeMismatchFails() {
        std::vector<float> v(84, 0.0f);
        v[0] = 160; v[1] = 120; v[2] = 100; v[3] = 100; v[4 + 5] = 0.9f;
        Tensor detect(v, std::vector<int64_t>{1, 84, 1});
        cv::Mat original(240, 320, CV_8UC3, cv::Scalar(20, 20, 20));
        // 元信息记录的原图尺寸（640x480）与实际原图（320x240）不一致
        std::vector<float> mv{0, 640, 480, 640, 480, 1, 0, 0};
        Tensor meta(mv, std::vector<int64_t>{1, 8});
        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_detect"},
            {"params", QVariantMap{{"conf", 0.25}}}},
            QVector<NodeData>{ detect, original, meta });
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("不一致")));
    }

    void yoloDetectMissingInputs() {
        std::vector<float> v(84, 0.0f);
        Tensor detect(v, std::vector<int64_t>{1, 84, 1});
        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_detect"},
            {"params", QVariantMap{{"conf", 0.25}}}}, QVector<NodeData>{ detect });
        QVERIFY(!r.ok);            // 缺「原图」「元信息」→ 报错
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

    // 分割解码：重叠框被类内 NMS 抑制，掩码系数从 [4+nc, C) 提取并随保留框带出
    void yoloSegmentDecodeSuppressesOverlapAndKeepsCoeffs() {
        const int64_t C = 7;   // 4 + nc(1) + nm(2)
        auto anchor = [&](std::vector<float>& v, int n, float cx, float cy,
                          float score, float c0, float c1) {
            v[n * C + 0] = cx; v[n * C + 1] = cy; v[n * C + 2] = 60; v[n * C + 3] = 60;
            v[n * C + 4] = score;
            v[n * C + 5] = c0;  v[n * C + 6] = c1;
        };
        std::vector<float> v(3 * C, 0.0f);
        anchor(v, 0, 100, 100, 0.90f, 1.0f, 2.0f);   // 高分框
        anchor(v, 1, 100, 100, 0.80f, 3.0f, 4.0f);   // 与 0 完全重叠 → 抑制
        anchor(v, 2, 300, 300, 0.70f, 5.0f, 6.0f);   // 独立框 → 保留
        Tensor head(v, std::vector<int64_t>{1, 3, C});   // channels-last

        std::vector<postprocess_detail::Detection> dets;
        QVERIFY(postprocess_detail::decodeDetections(head, 0.25f, 0.45f, 300, dets, 5));
        QCOMPARE(dets.size(), std::size_t(2));       // 重叠对只剩一个
        const postprocess_detail::Detection* top = nullptr;
        for (const auto& d : dets)
            if (!top || d.score > top->score) top = &d;
        QVERIFY(top);
        QCOMPARE(top->score, 0.90f);                 // 保留高分框
        QCOMPARE(int(top->coeff.size()), 2);         // nm 个系数
        QCOMPARE(top->coeff[0], 1.0f);               // 来自系数通道而非类别通道
        QCOMPARE(top->coeff[1], 2.0f);
        // 低分框被抑制，其系数（3,4）不应出现
        for (const auto& d : dets)
            QVERIFY(!(d.coeff[0] == 3.0f && d.coeff[1] == 4.0f));
    }

    // 分割任务：整图概率掩码须裁到该框 bbox，框外不着色
    void yoloSegmentMaskCroppedToBox() {
        const int64_t C = 6;   // 4 + nc(1) + nm(1)
        std::vector<float> hv(C, 0.0f);
        hv[0] = 50; hv[1] = 25; hv[2] = 100; hv[3] = 50;  // cx,cy,w,h → 上半幅
        hv[4] = 0.9f;                                     // 类别 0 分数
        hv[5] = 8.0f;                                     // 掩码系数：sigmoid(8)≈1
        Tensor head(hv, std::vector<int64_t>{1, 1, C});
        std::vector<float> pv{1, 1, 1, 1};                // 原型 2x2 全 1
        Tensor protos(pv, std::vector<int64_t>{1, 1, 2, 2});
        cv::Mat original(100, 100, CV_8UC3, cv::Scalar(0, 0, 0));
        std::vector<float> mv{0, 100, 100, 100, 100, 1, 0, 0};   // mode=0 恒等
        Tensor meta(mv, std::vector<int64_t>{1, 8});

        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_segment"},
            {"params", QVariantMap{{"conf", 0.25}, {"iou", 0.45}, {"maskThr", 0.5},
                                   {"alpha", 1.0}}}},
            QVector<NodeData>{ head, protos, original, meta });
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        QCOMPARE(out.cols, 100);
        QCOMPARE(out.rows, 100);
        // 框内（上半幅）着色为类别 0 的颜色；框外保持原图黑色
        QCOMPARE(out.at<cv::Vec3b>(25, 50), cv::Vec3b(255, 56, 56));
        QCOMPARE(out.at<cv::Vec3b>(80, 50), cv::Vec3b(0, 0, 0));
    }

    // 分割任务：缺输入报错（端口契约）
    void yoloSegmentMissingInputs() {
        std::vector<float> hv(6, 0.0f);
        hv[4] = 0.9f;
        Tensor head(hv, std::vector<int64_t>{1, 1, 6});
        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_segment"},
            {"params", QVariantMap{{"conf", 0.25}}}},
            QVector<NodeData>{ head });
        QVERIFY(!r.ok);
    }

    // 分割任务：框内只保留面积最大的连通掩码，细碎噪点被滤除
    void yoloSegmentKeepsLargestConnectedMask() {
        // 原型 24x24：主体（上 12 行）+ 右下角 8x8 碎块，其余为低值
        const int S = 24;
        std::vector<float> pv(std::size_t(S * S), -8.0f);
        for (int y = 0; y < 12; ++y)
            for (int x = 0; x < S; ++x)
                pv[std::size_t(y * S + x)] = 10.0f;
        for (int y = 16; y < S; ++y)
            for (int x = 16; x < S; ++x)
                pv[std::size_t(y * S + x)] = 10.0f;
        Tensor protos(pv, std::vector<int64_t>{1, 1, S, S});

        const int64_t C = 6;   // 4 + nc(1) + nm(1)
        std::vector<float> hv(C, 0.0f);
        hv[0] = 50; hv[1] = 50; hv[2] = 100; hv[3] = 100;  // 整图一个框
        hv[4] = 0.9f;
        hv[5] = 1.0f;                                       // 系数：sigmoid(±10/±8) 二值化清晰
        Tensor head(hv, std::vector<int64_t>{1, 1, C});
        cv::Mat original(100, 100, CV_8UC3, cv::Scalar(0, 0, 0));
        std::vector<float> mv{0, 100, 100, 100, 100, 1, 0, 0};
        Tensor meta(mv, std::vector<int64_t>{1, 8});

        PostProcessExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{{"task", "yolo_segment"},
            {"params", QVariantMap{{"conf", 0.25}, {"maskThr", 0.5}, {"alpha", 1.0}}}},
            QVector<NodeData>{ head, protos, original, meta });
        QVERIFY2(r.ok, qPrintable(r.error));
        const cv::Mat& out = std::get<cv::Mat>(r.outputs[0]);
        // 主体区域着色
        QCOMPARE(out.at<cv::Vec3b>(20, 50), cv::Vec3b(255, 56, 56));
        // 碎块被滤除，保持原图黑色
        QCOMPARE(out.at<cv::Vec3b>(90, 90), cv::Vec3b(0, 0, 0));
    }
};

QTEST_MAIN(TestTasks)
#include "test_tasks.moc"
