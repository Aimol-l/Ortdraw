#pragma once
#include <memory>
#include <string>
#include <vector>

#include "engine/NodeExecutor.hpp"
#include "engine/onnx/OnnxTensorConvert.hpp"
#include "onnx_engine/runtime.hpp"

// ONNX 推理执行器：按 params 取共享会话，张量/图像输入直转后 run，输出转回 NodeData
class OnnxInferExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&, const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const QString path = params.value("modelPath").toString();
        if (path.isEmpty()) return {false, QStringLiteral("未选择模型"), {}};

        onnx_engine::SessionOptions o;
        const QString dev = params.value("device", "auto").toString();
        o.device = dev == "cpu" ? onnx_engine::Device::CPU
                 : dev == "cuda" ? onnx_engine::Device::CUDA : onnx_engine::Device::Auto;
        o.intraThreads = params.value("threads", 0).toInt();

        std::string err;
        auto s = onnx_engine::Runtime::instance().session(path.toStdString(), o, err);
        if (!s) return {false, QString::fromStdString(err), {}};

        std::vector<onnx_engine::TensorBuffer> ins;
        for (const NodeData& d : inputs) {
            if (std::holds_alternative<Tensor>(d)) {
                auto b = onnx_convert::tensorToBuffer(std::get<Tensor>(d));
                if (b.data.empty()) return {false, QStringLiteral("输入张量无效"), {}};
                ins.push_back(std::move(b));
            } else if (std::holds_alternative<cv::Mat>(d)) {
                auto b = onnx_convert::imageToTensor(std::get<cv::Mat>(d), {}, onnx_engine::ElementType::Float32,
                                                     "none", {}, {}, "auto", "keep");
                if (b.data.empty())
                    return {false, QStringLiteral("图像输入无法直接转换，请先用「预处理」节点"), {}};
                ins.push_back(std::move(b));
            } else {
                return {false, QStringLiteral("输入必须是张量（请接「预处理」节点）"), {}};
            }
        }

        std::vector<onnx_engine::TensorBuffer> outs;
        if (!s->run(ins, outs, err)) return {false, QString::fromStdString(err), {}};

        ExecResult r;
        for (auto& b : outs) {
            NodeData d = onnx_convert::tensorToNodeData(b);
            if (std::holds_alternative<std::monostate>(d))
                return {false, QStringLiteral("输出类型不受支持"), {}};
            r.outputs.push_back(std::move(d));
        }
        return r;
    }
};
