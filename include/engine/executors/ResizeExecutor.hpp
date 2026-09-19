#pragma once
#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class ResizeExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        cv::Size size;
        if (params.value("mode").toInt() == 0) {
            size.width  = std::max(1, params.value("outWidth").toInt());
            size.height = std::max(1, params.value("outHeight").toInt());
        } else {
            const double pct = params.value("percent").toInt() / 100.0;
            size.width  = std::max(1, int(std::lround(img->cols * pct)));
            size.height = std::max(1, int(std::lround(img->rows * pct)));
        }

        cv::Mat out;
        cv::resize(*img, out, size, 0, 0, cv::INTER_LINEAR);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
