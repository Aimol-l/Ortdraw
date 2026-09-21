#pragma once
#include <functional>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include "engine/NodeExecutor.hpp"
#include "node/BaseNode.hpp"   // PortSpec

// 参数描述：任务 UI 依据它自动生成控件
struct ParamDesc {
    QString key, label;
    QString kind;                                   // "int" | "float" | "text" | "bool" | "select"
    QVariant def;
    QVector<QPair<QString, QVariant>> options;      // select 用
};

// 一个可注册的预处理/后处理任务：决定端口、默认参数与计算逻辑
struct TaskSpec {
    QString id, name;
    QVector<PortSpec> inputs, outputs;
    QVariantMap defaults;
    QVector<ParamDesc> params;
    std::function<ExecResult(const ExecuteContext&, const QVariantMap&, const QVector<NodeData>&)> compute;
};
