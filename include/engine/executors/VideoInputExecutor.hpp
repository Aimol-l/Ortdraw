#pragma once
#include <opencv2/videoio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "engine/NodeExecutor.hpp"

// 视频输入执行器：按帧号取一帧输出（BGR 8UC3，与 cv::imread 一致）。
// 同一路径的 cv::VideoCapture 会被缓存，避免每帧重新打开文件（seek 用
// CAP_PROP_POS_FRAMES）。图求值是单线程拓扑序，故 execute() 也按单线程
// 调用约定：同一 VideoCapture 的 seek/read 不会并发交错。若将来引入并行
// 求值，需为每条缓存条目加锁。
class VideoInputExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>&) const override {
        const QString path = params.value("path").toString();
        if (path.isEmpty())
            return {false, QStringLiteral("未选择视频文件"), {}};

        const int frame = params.value("frame", 0).toInt();

        std::shared_ptr<cv::VideoCapture> cap = captureFor(path);
        if (!cap->isOpened())
            return {false, QStringLiteral("无法打开视频（缺解码器？）: %1").arg(path), {}};

        const double total = cap->get(cv::CAP_PROP_FRAME_COUNT);
        if (frame < 0 || (total > 0 && frame >= static_cast<int>(total)))
            return {false, QStringLiteral("帧号超出范围（共 %1 帧）")
                               .arg(total > 0 ? QString::number(static_cast<int>(total))
                                              : QStringLiteral("?")), {}};

        if (!cap->set(cv::CAP_PROP_POS_FRAMES, frame))
            return {false, QStringLiteral("seek 到第 %1 帧失败").arg(frame), {}};

        cv::Mat img;
        if (!cap->read(img) || img.empty())
            return {false, QStringLiteral("读取第 %1 帧失败").arg(frame), {}};

        ExecResult r;
        r.outputs.push_back(img);
        return r;
    }

private:
    // 按路径缓存 VideoCapture：仅当缓存的 capture 已失效（isOpened() 为
    // false，如解码器异常）才重建；打不开的条目不入缓存，下次调用会重试。
    // 注意：Linux 下已打开的 capture 在文件被删除后 isOpened() 仍为 true，
    // 不会据此重建。
    static std::shared_ptr<cv::VideoCapture> captureFor(const QString& path) {
        static std::mutex mutex;
        static std::unordered_map<std::string, std::shared_ptr<cv::VideoCapture>> cache;
        std::lock_guard<std::mutex> lk(mutex);

        const std::string key = path.toStdString();
        auto it = cache.find(key);
        if (it != cache.end()) {
            if (it->second->isOpened()) return it->second;
            cache.erase(it);          // 缓存的 capture 已失效：重建
        }
        auto cap = std::make_shared<cv::VideoCapture>(key);
        if (cap->isOpened()) cache[key] = cap;
        return cap;                   // 打不开也返回，由调用方报错
    }
};
