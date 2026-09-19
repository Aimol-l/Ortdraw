#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class GrayExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        (void)params;
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        cv::Mat out;
        if (img->channels() == 1) out = *img;
        else cv::cvtColor(*img, out, cv::COLOR_BGR2GRAY);

        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
