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
        auto session = onnx_engine::Runtime::instance().session(path.toStdString(), o, err);
        if (!session) return {false, QString::fromStdString(err), {}};

        std::vector<Tensor> ins;
        for (const NodeData& d : inputs) {
            if (std::holds_alternative<Tensor>(d)) {
                ins.push_back(std::get<Tensor>(d));   // 零转换，直接作为推理输入
            } else if (std::holds_alternative<cv::Mat>(d)) {
                Tensor t = onnx_convert::imageToTensor(std::get<cv::Mat>(d), {},
                                                       onnx_engine::ElementType::Float32,
                                                       "none", {}, {}, "auto", "keep");
                if (t.numel() == 0)
                    return {false, QStringLiteral("图像输入无法直接转换，请先用「预处理」节点"), {}};
                ins.push_back(std::move(t));
            } else {
                return {false, QStringLiteral("输入必须是张量（请接「预处理」节点）"), {}};
            }
        }

        auto res = session->run(ins);
        if (!res)
            return {false, QString::fromStdString(res.error()), {}};
        const std::vector<Tensor>& outs = *res;

        const auto& outInfos = session->info().outputs;

        ExecResult result;
        for (std::size_t i = 0; i < outs.size(); ++i) {
            const onnx_engine::ElementType declType =
                (i < outInfos.size()) ? outInfos[i].type : onnx_engine::ElementType::Unknown;

            NodeData d = onnx_convert::tensorToNodeData(outs[i], declType);

            if (std::holds_alternative<std::monostate>(d))
                return {false, QStringLiteral("输出类型不受支持"), {}};
            
            result.outputs.push_back(std::move(d));
        }
        return result;
    }
};
