#pragma once
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

        int alpha = params.value("alpha").toInt();
        alpha = qBound(0, alpha, 100);

        cv::Mat bresized;
        if (a->size() != b->size()) {
            cv::resize(*b, bresized, a->size(), 0, 0, cv::INTER_LINEAR);
        } else {
            bresized = *b;
        }

        cv::Mat out;
        cv::addWeighted(*a, alpha / 100.0, bresized, 1.0 - alpha / 100.0, 0, out);
        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
