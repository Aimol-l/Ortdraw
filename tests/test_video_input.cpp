#include <QtTest>
#include <QFileInfo>
#include "engine/executors/VideoInputExecutor.hpp"

class TestVideoInput : public QObject {
    Q_OBJECT
private slots:
    // 未选择文件：明确报错，不崩溃
    void emptyPathFails() {
        VideoInputExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{}, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("未选择视频")));
        QVERIFY(r.outputs.isEmpty());
    }

    // 文件不存在 / 打不开：明确报错
    void missingFileFails() {
        VideoInputExecutor ex;
        QVariantMap p;
        p["path"] = QStringLiteral("/tmp/opencode/no-such-video.mkv");
        p["frame"] = 0;
        const ExecResult r = ex.execute({}, p, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("无法打开")));
    }

    // 真实素材：帧号越界必须报错（而不是返回空图或崩溃）
    void frameOutOfRangeFails() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        QVariantMap p;
        p["path"] = path;
        p["frame"] = 999999;
        const ExecResult r = ex.execute({}, p, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("帧号超出范围")));
    }

    // 真实素材：seek 到不同帧都能读出 1744x982 的 BGR 8UC3
    void readsFramesFromAsset() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        for (int f : {0, 30, 900}) {
            QVariantMap p;
            p["path"] = path;
            p["frame"] = f;
            const ExecResult r = ex.execute({}, p, {});
            QVERIFY2(r.ok, r.error.toStdString().c_str());
            QCOMPARE(r.outputs.size(), 1);
            const cv::Mat m = std::get<cv::Mat>(r.outputs[0]);   // 与仓库既有测试一致
            QCOMPARE(m.cols, 1744);
            QCOMPARE(m.rows, 982);
            QCOMPARE(m.type(), CV_8UC3);
            QVERIFY(!m.empty());
        }
    }

    // 两次读同一帧结果一致（seek 语义稳定）
    void sameFrameIsStable() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        auto readAt = [&](int f) -> cv::Mat {
            QVariantMap p;
            p["path"] = path;
            p["frame"] = f;
            const ExecResult r = ex.execute({}, p, {});
            if (!r.ok || r.outputs.isEmpty()) return {};
            return std::get<cv::Mat>(r.outputs[0]).clone();
        };
        const cv::Mat a = readAt(120);
        QVERIFY2(!a.empty(), "readAt(120) 失败");
        const cv::Mat b = readAt(120);
        QCOMPARE(cv::norm(a, b), 0.0);
    }
};

QTEST_MAIN(TestVideoInput)
#include "test_video_input.moc"
