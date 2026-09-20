#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

// 裁剪：x/y 为左上角坐标(>=0)，w/h 为子图大小(>0；为 0 时取到图像边界)。
// 要求 x+w <= 图像宽 且 y+h <= 图像高，越界或非法时报错。
class CropExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};
        if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

        const int x = params.value("x", 0).toInt();
        const int y = params.value("y", 0).toInt();
        int w = params.value("w", 0).toInt();
        int h = params.value("h", 0).toInt();

        if (x < 0 || y < 0)
            return {false, QStringLiteral("裁剪起点 x/y 不能为负"), {}};
        if (x >= img->cols || y >= img->rows)
            return {false, QStringLiteral("裁剪起点超出图像范围"), {}};

        // w/h <= 0 表示取到图像边界
        w = (w > 0) ? w : (img->cols - x);
        h = (h > 0) ? h : (img->rows - y);

        if (x + w > img->cols)
            return {false, QStringLiteral("裁剪区域超出图像宽度：x+w=%1 > W=%2")
                               .arg(x + w).arg(img->cols), {}};
        if (y + h > img->rows)
            return {false, QStringLiteral("裁剪区域超出图像高度：y+h=%1 > H=%2")
                               .arg(y + h).arg(img->rows), {}};

        cv::Mat out = (*img)(cv::Rect(x, y, w, h)).clone();
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
