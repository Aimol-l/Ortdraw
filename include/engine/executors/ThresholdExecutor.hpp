#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class ThresholdExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        cv::Mat gray;
        if (img->channels() == 1) gray = *img;
        else cv::cvtColor(*img, gray, cv::COLOR_BGR2GRAY);

        const int thr = params.value("threshold").toInt();
        cv::Mat out;
        cv::threshold(gray, out, thr, 255, cv::THRESH_BINARY);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
