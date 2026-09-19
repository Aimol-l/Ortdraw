#pragma once
#include "engine/NodeExecutor.hpp"

class ImageShowExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap&,
                       const QVector<NodeData>& inputs) const override {
        if (inputs.isEmpty() || !std::holds_alternative<cv::Mat>(inputs[0]))
            return {false, QStringLiteral("输入不是图像"), {}};

        ExecResult r;
        r.outputs.push_back(inputs[0]);
        return r;
    }
};
