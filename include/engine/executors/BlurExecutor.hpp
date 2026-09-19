#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class BlurExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        int k = params.value("kernel").toInt();
        if (k < 1) k = 1;
        if (k % 2 == 0) ++k;

        cv::Mat out;
        cv::GaussianBlur(*img, out, cv::Size(k, k), 0);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
