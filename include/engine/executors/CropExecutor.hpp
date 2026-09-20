#pragma once
#include <algorithm>
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class CropExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};
        if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

        int x = params.value("x", 0).toInt();
        int y = params.value("y", 0).toInt();
        int w = params.value("w", 0).toInt();
        int h = params.value("h", 0).toInt();

        x = std::max(0, x);
        y = std::max(0, y);
        if (x >= img->cols || y >= img->rows)
            return {false, QStringLiteral("裁剪区域无效"), {}};

        // w/h <= 0 表示取到图像边界
        w = (w > 0) ? w : (img->cols - x);
        h = (h > 0) ? h : (img->rows - y);
        w = std::min(w, img->cols - x);
        h = std::min(h, img->rows - y);
        if (w <= 0 || h <= 0)
            return {false, QStringLiteral("裁剪区域无效"), {}};

        cv::Mat out = (*img)(cv::Rect(x, y, w, h)).clone();
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
