#pragma once
#include <vector>
#include "engine/NodeExecutor.hpp"
#include "engine/executors/ImageConvert.hpp"

// 张量：把 rows/cols/data 参数转换为 FLOAT32 的 2D 张量输出
class TensorExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>&) const override {
        const int rows = params.value("rows").toInt();
        const int cols = params.value("cols").toInt();
        if (rows <= 0 || cols <= 0)
            return {false, QStringLiteral("张量尺寸无效"), {}};

        const QVariantList dl = params.value("data").toList();
        if (dl.size() != rows * cols)
            return {false, QStringLiteral("张量数据尺寸不匹配"), {}};

        std::vector<float> vec;
        vec.reserve(dl.size());
        for (const QVariant& v : dl) vec.push_back(v.toFloat());

        std::vector<int64_t> shape{ rows, cols };
        Tensor t(vec, shape);

        ExecResult r;
        r.outputs.push_back(NodeData{t});
        return r;
    }
};
