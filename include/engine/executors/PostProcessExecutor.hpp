#pragma once
#include "engine/NodeExecutor.hpp"
#include "engine/tasks/PostProcessRegistry.hpp"

// 后处理执行器：按 params.task 找到 TaskSpec，合并默认参数后转发 compute
class PostProcessExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext& ctx, const QVariantMap& params, const QVector<NodeData>& inputs) const override {

        const auto* spec = PostProcessRegistry::instance().find(params.value("task").toString());

        if (!spec) return {false, QStringLiteral("未知后处理任务"), {}};

        QVariantMap merged = spec->defaults;
        const QVariantMap p = params.value("params").toMap();

        for (auto it = p.begin(); it != p.end(); ++it) 
            merged[it.key()] = it.value();
        
        return spec->compute(ctx, merged, inputs);
    }
};
