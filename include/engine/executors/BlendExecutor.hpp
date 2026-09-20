#pragma once
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class BlendExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* a = imageInput(inputs, 0);
        const cv::Mat* b = imageInput(inputs, 1);
        if (!a || !b) return {false, QStringLiteral("需要两张图像输入"), {}};
        if (a->size() != b->size())
            return {false, QStringLiteral("两张图像尺寸不一致"), {}};

        int alpha = params.value("alpha").toInt();
        alpha = qBound(0, alpha, 100);

        // 通道归一：低通道数的图对齐到高通道数（灰度升为彩/带 alpha）
        cv::Mat an = *a, bn = *b;
        if (a->channels() != b->channels()) {
            const int target = std::max(a->channels(), b->channels());
            auto upcast = [](const cv::Mat& src, cv::Mat& dst, int c) -> bool {
                if (src.channels() == c) { dst = src; return true; }
                if (c == 3 && src.channels() == 1) { cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR); return true; }
                if (c == 4 && src.channels() == 1) { cv::cvtColor(src, dst, cv::COLOR_GRAY2BGRA); return true; }
                if (c == 4 && src.channels() == 3) { cv::cvtColor(src, dst, cv::COLOR_BGR2BGRA); return true; }
                return false;
            };
            if (!upcast(*a, an, target) || !upcast(*b, bn, target))
                return {false, QStringLiteral("不支持的通道组合"), {}};
        }

        cv::Mat out;
        cv::addWeighted(an, alpha / 100.0, bn, 1.0 - alpha / 100.0, 0, out);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
