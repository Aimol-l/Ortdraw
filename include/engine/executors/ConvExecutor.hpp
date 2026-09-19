#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

// 卷积：输出统一为 CV_8U，便于与其余图像节点互操作。
class ConvExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        (void)params;
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        const Tensor* kernel = nullptr;
        for (const auto& d : inputs) {
            if (std::holds_alternative<Tensor>(d)) {
                kernel = &std::get<Tensor>(d);
                break;
            }
        }
        if (!kernel) return {false, QStringLiteral("缺少卷积核"), {}};

        if (kernel->dtype() != via::DataType::FLOAT32 || kernel->shape().size() != 2)
            return {false, QStringLiteral("卷积核必须是 FLOAT32 的 2D 张量"), {}};

        const cv::Mat k = tensorToMat(*kernel);
        if (k.empty()) return {false, QStringLiteral("卷积核无效"), {}};

        cv::Mat f;
        img->convertTo(f, CV_32F);
        cv::Mat out;
        cv::filter2D(f, out, CV_32F, k);
        out.convertTo(out, CV_8U);

        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
