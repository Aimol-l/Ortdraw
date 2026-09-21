#pragma once
#include <QString>
#include <QVariantMap>
#include <QVector>
#include <atomic>
#include <functional>
#include "engine/NodeData.hpp"

struct ExecuteContext {
    QString nodeUuid;
    std::atomic_bool* cancel = nullptr;
    std::function<void(const QString&)> log;
    std::function<void(const QVariantMap&)> display;   // 非端口显示数据
};

struct ExecResult {
    bool ok = true;
    QString error;
    QVector<NodeData> outputs;
};

class NodeExecutor {
public:
    virtual ~NodeExecutor() = default;
    virtual QVariantMap defaultParams() const { return {}; }
    virtual ExecResult execute(const ExecuteContext& ctx,
                               const QVariantMap& params,
                               const QVector<NodeData>& inputs) const = 0;
};
