#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class MorphologyExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        int k = params.value("kernel").toInt();
        if (k < 3) k = 3;
        if (k % 2 == 0) ++k;
        int iterations = params.value("iterations").toInt();
        if (iterations < 1) iterations = 1;

        const cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(k, k));
        const int op = params.value("op").toInt();

        cv::Mat out;
        switch (op) {
            case 0: cv::erode(*img, out, kernel, cv::Point(-1, -1), iterations); break;
            case 1: cv::dilate(*img, out, kernel, cv::Point(-1, -1), iterations); break;
            case 2: cv::morphologyEx(*img, out, cv::MORPH_OPEN, kernel, cv::Point(-1, -1), iterations); break;
            case 3: cv::morphologyEx(*img, out, cv::MORPH_CLOSE, kernel, cv::Point(-1, -1), iterations); break;
            case 4: cv::morphologyEx(*img, out, cv::MORPH_GRADIENT, kernel, cv::Point(-1, -1), iterations); break;
            default: return {false, QStringLiteral("未知的形态学操作"), {}};
        }

        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
