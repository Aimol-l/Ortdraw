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
    QString kind;                                   // "int" | "float" | "text" | "bool" | "select" | "floats"
    QVariant def;
    QVector<QPair<QString, QVariant>> options;      // select 用
    int group = 0;                                  // >0：与同 group 的参数排在同一行
    int vecCount = 0;                               // kind=="floats"：输入框数量（按逗号拆分）
    QString showIfKey;                              // 非空：仅当 params[showIfKey]==showIfValue 时显示
    QString showIfValue;
};

// 一个可注册的预处理/后处理任务：决定端口、默认参数与计算逻辑
struct TaskSpec {
    QString id, name;
    QVector<PortSpec> inputs, outputs;
    QVariantMap defaults;
    QVector<ParamDesc> params;
    std::function<ExecResult(const ExecuteContext&, const QVariantMap&, const QVector<NodeData>&)> compute;
};
