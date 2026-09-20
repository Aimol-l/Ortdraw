#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class FlipRotateExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};
        if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

        const int mode = params.value("mode", 0).toInt();
        cv::Mat out;
        switch (mode) {
        case 0: cv::flip(*img, out, 1); break;                              // 水平翻转
        case 1: cv::flip(*img, out, 0); break;                              // 垂直翻转
        case 2: cv::rotate(*img, out, cv::ROTATE_90_CLOCKWISE); break;
        case 3: cv::rotate(*img, out, cv::ROTATE_180); break;
        case 4: cv::rotate(*img, out, cv::ROTATE_90_COUNTERCLOCKWISE); break; // 270° 顺时针
        default: return {false, QStringLiteral("无效的翻转/旋转模式"), {}};
        }

        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
