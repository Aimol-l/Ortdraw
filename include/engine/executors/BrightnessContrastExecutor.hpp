#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class BrightnessContrastExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};
        if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

        const int brightness = params.value("brightness", 0).toInt();
        const int contrast = params.value("contrast", 100).toInt();

        cv::Mat out;
        img->convertTo(out, -1, contrast / 100.0, brightness);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
