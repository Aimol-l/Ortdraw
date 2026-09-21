# ONNX 集成到 Ortdraw 实施计划（适配层 / 任务框架 / 三种节点）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Ortdraw 中提供「ONNX 推理」「预处理」「后处理」三种节点：ONNX 节点按模型元数据动态生成张量端口；预处理/后处理用可注册任务框架（任务决定端口与参数，UI 自动生成控件）；分类结果经非端口显示通道展示。

**Architecture:** 应用侧新增 `OnnxTensorConvert`（`onnx_engine` 中性张量 ↔ `cv::Mat`/`Tensorvia::Tensor`，含 dtype 映射）、`BaseNode::rebuildPorts`（动态端口 + 断线）、任务框架 `TaskSpec` + 两个注册表与两个执行器、`GraphExecutor::nodeDisplay` 显示通道；节点 QML 复用 `TaskNodeControls` 自动渲染参数。

**Tech Stack:** C++23 / Qt6 Quick / OpenCV / Tensorvia / onnxruntime（经 `onnx_engine`）。

**前置：** SDK 分支 `feat/onnx-engine` 上 SDK 已完成（`libonnx_engine.so`，`ctest 18/18`）。

**设计文档：** `docs/superpowers/specs/2026-09-21-ortdraw-onnx-engine-design.md`

**通用命令：** `cmake --build build -j4`；`ctest --test-dir build --output-on-failure`；`/usr/lib/qt6/bin/qmllint <file>`。实机模型：`models/yolo11n.onnx`（动态维，gitignored，已下载）。

---

## 文件结构

- Create: `include/engine/onnx/OnnxTensorConvert.hpp`
- Modify: `include/node/BaseNode.hpp`（`PortSpec` + `rebuildPorts`）
- Modify: `include/NodeManager.h`（端口重建断线辅助）
- Modify: `include/engine/NodeExecutor.hpp`（`ExecuteContext::display`）
- Modify: `include/engine/GraphExecutor.hpp`（`nodeDisplay` 信号 + 回调装配）
- Modify: `include/NodeManager.h`（转发 display / 清空）
- Create: `include/engine/tasks/TaskSpec.hpp`
- Create: `include/engine/tasks/PreProcessRegistry.hpp` + `PostProcessRegistry.hpp`
- Create: `include/engine/executors/OnnxInferExecutor.hpp`、`PreProcessExecutor.hpp`、`PostProcessExecutor.hpp`
- Create: `include/node/OnnxInfer.hpp`、`PreProcess.hpp`、`PostProcess.hpp`
- Create: `qml/node/OnnxInferNode.qml`、`PreProcessNode.qml`、`PostProcessNode.qml`、`qml/node/TaskNodeControls.qml`
- Modify: `qml/palette/NodeCatalog.qml`、`assets.qrc`、`include/engine/BuiltinExecutors.hpp`
- Tests: `tests/test_onnx_convert.cpp`、`tests/test_tasks.cpp`、`tests/test_onnx_nodes.cpp`

---

## Task 1: ONNX ↔ 应用类型适配（含 dtype 映射）

**Files:** Create `include/engine/onnx/OnnxTensorConvert.hpp`; Create `tests/test_onnx_convert.cpp`; Modify `tests/CMakeLists.txt`（链接已含 onnx_engine；TEST_HEADERS 加该头）

- [ ] **Step 1: 写失败测试 `tests/test_onnx_convert.cpp`**

```cpp
#include <QtTest>
#include "engine/onnx/OnnxTensorConvert.hpp"
#include <opencv2/imgproc.hpp>

class TestOnnxConvert : public QObject {
    Q_OBJECT
private slots:
    void dtypeMappingRoundTrip() {
        using namespace onnx_convert;
        QCOMPARE(toViaType(onnx_engine::ElementType::Float32), via::DataType::FLOAT32);
        QCOMPARE(toViaType(onnx_engine::ElementType::Int64),   via::DataType::INT64);
        QCOMPARE(toViaType(onnx_engine::ElementType::UInt8),   via::DataType::INT16);  // 无符号→有符号16
        QCOMPARE(toViaType(onnx_engine::ElementType::Bool),    via::DataType::INT8);   // bool→int8
        QCOMPARE(fromViaType(via::DataType::FLOAT32), onnx_engine::ElementType::Float32);
        QCOMPARE(fromViaType(via::DataType::INT16),   onnx_engine::ElementType::Int16);
    }

    void imageToTensorNchwDiv255() {
        cv::Mat bgr(2, 3, CV_8UC3, cv::Scalar(0, 0, 255));   // BGR 全红
        const auto t = onnx_convert::imageToTensor(bgr, {1, 3, 2, 3},
                                                   onnx_engine::ElementType::Float32,
                                                   "div255", {}, {}, "rgb", "keep");
        QCOMPARE(t.shape, (std::vector<int64_t>{1, 3, 2, 3}));
        QCOMPARE(t.type, onnx_engine::ElementType::Float32);
        const float* p = reinterpret_cast<const float*>(t.data.data());
        // NCHW：R 通道全 1.0（BGR 红 -> RGB 后 R=255 -> /255=1）
        QCOMPARE(p[0], 1.0f);
    }

    void tensorToNodeDataFloat() {
        onnx_engine::TensorBuffer b; b.type = onnx_engine::ElementType::Float32;
        b.shape = {2, 2}; b.data.resize(4 * 4);
        float vals[4] = {1, 2, 3, 4};
        std::memcpy(b.data.data(), vals, sizeof(vals));
        const NodeData d = onnx_convert::tensorToNodeData(b);
        QVERIFY(std::holds_alternative<Tensor>(d));
        QCOMPARE(std::get<Tensor>(d).shape().size(), size_t(2));
    }

    void tensorToNodeDataBoolAndInt() {
        onnx_engine::TensorBuffer bb; bb.type = onnx_engine::ElementType::Bool;
        bb.shape = {}; bb.data = {1};
        QVERIFY(std::holds_alternative<bool>(onnx_convert::tensorToNodeData(bb)));
        onnx_engine::TensorBuffer bi; bi.type = onnx_engine::ElementType::Int64;
        bi.shape = {}; bi.data.resize(8); int64_t v = 7; std::memcpy(bi.data.data(), &v, 8);
        const NodeData d = onnx_convert::tensorToNodeData(bi);
        QVERIFY(std::holds_alternative<double>(d));
        QCOMPARE(std::get<double>(d), 7.0);
    }
};

QTEST_MAIN(TestOnnxConvert)
#include "test_onnx_convert.moc"
```

- [ ] **Step 2: 运行确认失败**（`OnnxTensorConvert.hpp` 不存在）
- [ ] **Step 3: 实现 `include/engine/onnx/OnnxTensorConvert.hpp`**

关键函数（签名固定，实现见下）：
```cpp
#pragma once
#include <opencv2/imgproc.hpp>
#include <tensorvia/core/tensor.h>
#include "engine/NodeData.hpp"
#include "onnx_engine/types.hpp"

namespace onnx_convert {

// ONNX ElementType ↔ Tensorvia DataType（UInt8→INT16、Bool→INT8）
inline via::DataType toViaType(onnx_engine::ElementType t);
inline onnx_engine::ElementType fromViaType(via::DataType t);

// cv::Mat(BGR/灰度, 8U) → 模型输入张量（布局 NCHW/NHWC、通道 rgb/bgr/auto、归一化 none/div255、
// 尺寸 resize: auto(按 targetShape 的空间维) / keep）。resize 插值用 INTER_LINEAR。
onnx_engine::TensorBuffer imageToTensor(const cv::Mat& img,
                                        const std::vector<int64_t>& targetShape,
                                        onnx_engine::ElementType dtype,
                                        const QString& norm, const QVector<double>& mean,
                                        const QVector<double>& std,
                                        const QString& channel, const QString& resize);

// Tensorvia::Tensor → 中性张量
onnx_engine::TensorBuffer tensorToBuffer(const Tensor& t);

// 中性张量 → NodeData：float/double/float16 → Tensor；Bool 标量 → bool；整型标量 → double；否则 Tensor
NodeData tensorToNodeData(const onnx_engine::TensorBuffer& b);

} // namespace onnx_convert
```
（实现要点：`toViaType` 用 switch 覆盖 §4.1 表；`imageToTensor` 先按 `channel` 做 BGR→RGB，再 `convertTo(CV_32F)` 并按 `norm` 归一化，随后按目标形状空间维 resize，最后按布局（`targetShape` 为 4 维且 `shape[1]==3` 视为 NCHW，否则 NHWC）写入；`tensorToNodeData` 用 `elementTypeSize` 与元素个数判断标量。）

- [ ] **Step 4: 运行确认通过** → `cmake --build build -j4 && ctest --test-dir build -R test_onnx_convert --output-on-failure`
- [ ] **Step 5: 提交** `git add include/engine/onnx/OnnxTensorConvert.hpp tests/test_onnx_convert.cpp tests/CMakeLists.txt && git commit -m "feat(onnx): tensor/image conversion adapters with dtype mapping"`

---

## Task 2: `BaseNode::rebuildPorts` + 端口变更断线

**Files:** Modify `include/node/BaseNode.hpp`、`include/NodeManager.h`；Create `tests/test_rebuild_ports.cpp`

- [ ] **Step 1: 写失败测试 `tests/test_rebuild_ports.cpp`**

```cpp
#include <QtTest>
#include "node/OnnxInfer.hpp"   // 或任一具体节点；本任务先用一个临时子类
#include "node/BaseNode.hpp"

class DummyNode : public BaseNode {
    Q_OBJECT
public:
    QString typeName() const override { return "Dummy"; }
    DummyNode() { m_input_ports.push_back(new Port("in0", PortType::Input, DataType::Tensor, {}, this)); }
};

class TestRebuildPorts : public QObject {
    Q_OBJECT
private slots:
    void rebuildReplacesPorts() {
        DummyNode n;
        QCOMPARE(n.getInPorts().size(), 1);
        n.rebuildPorts({{"a", DataType::Tensor}, {"b", DataType::Image}},
                       {{"out0", DataType::Tensor}});
        QCOMPARE(n.getInPorts().size(), 2);
        QCOMPARE(n.getOutPorts().size(), 1);
        QCOMPARE(n.getInPorts()[1]->dataType(), DataType::Image);
        QCOMPARE(n.getInPorts()[1]->name(), QString("b"));
    }
};
QTEST_MAIN(TestRebuildPorts)
#include "test_rebuild_ports.moc"
```

- [ ] **Step 2: 实现 `BaseNode`**

```cpp
struct PortSpec { QString name; DataType type; };
// public:
void rebuildPorts(const QVector<PortSpec>& ins, const QVector<PortSpec>& outs) {
    qDeleteAll(m_input_ports);  m_input_ports.clear();
    qDeleteAll(m_output_ports); m_output_ports.clear();
    for (const auto& s : ins)
        m_input_ports.push_back(new Port(s.name, PortType::Input, s.type, QPointF(0,0), this));
    for (const auto& s : outs)
        m_output_ports.push_back(new Port(s.name, PortType::Output, s.type, QPointF(0,0), this));
    emit inputPortsChanged();
    emit outputPortsChanged();
}
```
（`Port` 需有 `name()`/`dataType()` 访问器；若无则补。）

- [ ] **Step 3: NodeManager 增断开辅助**（供节点换模型/换任务时调用，走现有删边命令保持撤销一致）

```cpp
// 断开所有挂在这些端口上的边；mode 由调用方保证端口对象仍有效
Q_INVOKABLE void disconnectPorts(const QVariantList& /*uuid+index*/) {}
```
> 实际实现：节点在重建前通过信号把「将被移除的输入/输出端口索引」告知 `NodeManager`（或 `NodeManager::rebuildNodePorts(node, specs...)` 统一负责：先遍历 `getAllEdges()` 删除涉及旧端口的边（`RemoveEdgeCMD`），再调用 `node->rebuildPorts(...)`）。计划推荐后者：
```cpp
Q_INVOKABLE void rebuildNodePorts(BaseNode* node, const QVariantList& ins, const QVariantList& outs);
```

- [ ] **Step 4: 测试通过 + 提交**
`cmake --build build -j4 && ctest --test-dir build -R test_rebuild_ports --output-on-failure`
`git add include/node/BaseNode.hpp include/NodeManager.h tests/test_rebuild_ports.cpp tests/CMakeLists.txt && git commit -m "feat: dynamic port rebuild with edge cleanup"`

---

## Task 3: 显示通道（非端口）

**Files:** Modify `include/engine/NodeExecutor.hpp`、`include/engine/GraphExecutor.hpp`、`include/NodeManager.h`

- [ ] **Step 1: `ExecuteContext` 增加显示回调**

```cpp
struct ExecuteContext {
    QString nodeUuid;
    std::atomic_bool* cancel = nullptr;
    std::function<void(const QString&)> log;
    std::function<void(const QVariantMap&)> display;   // 非端口显示数据
};
```

- [ ] **Step 2: `GraphExecutor` 增加信号并装配回调**

```cpp
signals:
    void nodeDisplay(const QString& uuid, const QVariantMap& data);
```
在 worker 调用执行器处（`GraphExecutor.hpp` 约 275 行）：
```cpp
ExecuteContext ctx{ns.uuid, &m_cancel, [](const QString&){},
                   [this, runId, uuid](const QVariantMap& d) {
    QMetaObject::invokeMethod(this, [this, runId, uuid, d] {
        if (runId != m_run_id.load()) return;
        emit nodeDisplay(uuid, d);
    }, Qt::QueuedConnection);
}};
```

- [ ] **Step 3: `NodeManager` 转发并清空**

```cpp
QObject::connect(&m_executor, &GraphExecutor::nodeDisplay, this,
    [this](const QString& uuid, const QVariantMap& d) {
        if (BaseNode* n = nodeByUuid(uuid)) {
            QMetaObject::invokeMethod(n, "setDisplayData", Qt::DirectConnection, Q_ARG(QVariantMap, d));
        }
    });
```
并在 `run()` 开始清空各节点显示数据（`m_paint_board` 遍历调用 `setDisplayData({})`）。
（要求节点类提供 `Q_INVOKABLE void setDisplayData(const QVariantMap&)`。）

- [ ] **Step 4: 提交** `git add include/engine/NodeExecutor.hpp include/engine/GraphExecutor.hpp include/NodeManager.h && git commit -m "feat(engine): non-port node display channel"`

---

## Task 4: 任务框架 + 预处理节点

**Files:** Create `include/engine/tasks/TaskSpec.hpp`、`PreProcessRegistry.hpp`、`PostProcessRegistry.hpp`、`include/engine/executors/PreProcessExecutor.hpp`、`PostProcessExecutor.hpp`、`include/node/PreProcess.hpp`、`PostProcess.hpp`；Create `tests/test_tasks.cpp`

- [ ] **Step 1: `TaskSpec`**

```cpp
#pragma once
#include <functional>
#include <QString>
#include <QVariantMap>
#include <QVector>
#include "engine/NodeExecutor.hpp"
#include "node/BaseNode.hpp"   // PortSpec

struct ParamDesc {
    QString key, label;
    QString kind;              // "int" | "float" | "text" | "bool" | "select"
    QVariant def;
    QVector<QPair<QString, QVariant>> options;   // select 用
};

struct TaskSpec {
    QString id, name;
    QVector<PortSpec> inputs, outputs;
    QVariantMap defaults;
    QVector<ParamDesc> params;
    // 统一带 ExecuteContext（分类等可经 ctx.display 上报显示数据）
    std::function<ExecResult(const ExecuteContext&, const QVariantMap&, const QVector<NodeData>&)> compute;
};
```

- [ ] **Step 2: 两个注册表（单例，便于 QML 枚举）**

```cpp
class PreProcessRegistry {
public:
    static PreProcessRegistry& instance();
    void add(TaskSpec spec);
    const QVector<TaskSpec>& all() const;
    const TaskSpec* find(const QString& id) const;
};
// PostProcessRegistry 同构
```

- [ ] **Step 3: 执行器（转发）**

```cpp
class PreProcessExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext& ctx, const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const auto* spec = PreProcessRegistry::instance().find(params.value("task").toString());
        if (!spec) return {false, QStringLiteral("未知预处理任务"), {}};
        QVariantMap merged = spec->defaults;
        const QVariantMap p = params.value("params").toMap();
        for (auto it = p.begin(); it != p.end(); ++it) merged[it.key()] = it.value();
        return spec->compute(ctx, merged, inputs);
    }
};
```
（`PostProcessExecutor` 同构；两者在 `registerBuiltinExecutors()` 注册为 `"PreProcess"`/`"PostProcess"`。）

- [ ] **Step 4: 内置预处理任务（`图像→张量`、`YOLO letterbox`、`归一化`、`Resize`）**
  - `image_to_tensor`：端口 `[图像 Image] → [张量 Tensor]`；参数 `layout(NCHW/NHWC)`、`channel(auto/rgb/bgr)`、`norm(none/div255/meanstd)`、`mean/std`（文本，逗号分隔）、`resize(auto/keep)`、`width/height`；`compute` 取 `cv::Mat`，调 `onnx_convert::imageToTensor(..., targetShape 由 width/height/layout 组装)`。
  - `yolo_letterbox`：参数 `size(默认 640)`、`pad(114)`；输出 `[1,3,size,size]` float32，等比缩放 + 居中填充。
  - `normalize` / `resize`：输入输出均为 `Tensor`，分别做逐元素归一化与尺寸变换（用 `cv::Mat` 中转）。

- [ ] **Step 5: 预处理节点 `PreProcessNode`**

```cpp
class PreProcessNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString task READ task WRITE setTask NOTIFY paramsChanged)
public:
    QString typeName() const override { return "PreProcess"; }
    QString category() const override { return "process"; }
    Q_INVOKABLE QVariantList taskOptions() const;     // [{id,name}] 供下拉
    Q_INVOKABLE QVariantList paramDescs() const;      // 当前任务的 ParamDesc（供 UI 生成控件）
    Q_INVOKABLE QVariantMap taskParams() const;
    Q_INVOKABLE void setTaskParam(const QString& k, const QVariant& v);
    void setTask(const QString& id);                  // 触发 rebuildPorts（经 NodeManager）
    QVariantMap params() const override;              // { task, params }
    void setParams(const QVariantMap&) override;
    Q_INVOKABLE void setDisplayData(const QVariantMap&) {}   // 无显示
signals:
    void portsRebuildRequested();   // NodeManager 连接：先断边再 rebuildPorts
};
```
> `setTask` 通过 `NodeManager::rebuildNodePorts` 更新端口（保证断线）。

- [ ] **Step 6: 测试 `tests/test_tasks.cpp`**
  - `imageToTensorTask`：构造 `cv::Mat`，params `{task:"image_to_tensor", width:2, height:2, layout:"NCHW", channel:"rgb", norm:"div255"}` → 断言输出 `Tensor` 的 shape `{1,3,2,2}` 与首值。
  - `registryHasTasks`：断言 `PreProcessRegistry` 含上述 id。
  - `postProcessRegistryHasTasks`：含 `yolo_detect`/`yolo_segment`/`classify`。

- [ ] **Step 7: 提交** `git add include/engine/tasks include/engine/executors/PreProcessExecutor.hpp include/engine/executors/PostProcessExecutor.hpp include/node/PreProcess.hpp include/node/PostProcess.hpp tests/test_tasks.cpp tests/CMakeLists.txt && git commit -m "feat(tasks): task framework with preprocessing tasks and node"`

---

## Task 5: ONNX 推理节点与执行器

**Files:** Create `include/node/OnnxInfer.hpp`、`include/engine/executors/OnnxInferExecutor.hpp`；Create `tests/test_onnx_nodes.cpp`

- [ ] **Step 1: `OnnxInferNode`**（参数 `modelPath/device/threads`；载入模型 → `Runtime::modelInfo` → `rebuildPorts`（输入 `Tensor`，输出 `Tensor`，名=模型 IO 名）；tooltip 用 `ioText` 暴露 dtype/shape；`Q_INVOKABLE void reloadModel()`）
- [ ] **Step 2: `OnnxInferExecutor`**

```cpp
class OnnxInferExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&, const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const std::string path = params.value("modelPath").toString().toStdString();
        if (path.empty()) return {false, QStringLiteral("未选择模型"), {}};
        onnx_engine::SessionOptions o;
        const QString dev = params.value("device", "auto").toString();
        o.device = dev == "cpu" ? onnx_engine::Device::CPU
                 : dev == "cuda" ? onnx_engine::Device::CUDA : onnx_engine::Device::Auto;
        o.intraThreads = params.value("threads", 0).toInt();
        std::string err;
        auto s = onnx_engine::Runtime::instance().session(path, o, err);
        if (!s) return {false, QString::fromStdString(err), {}};
        std::vector<onnx_engine::TensorBuffer> ins;
        for (const NodeData& d : inputs) {
            if (std::holds_alternative<Tensor>(d)) ins.push_back(onnx_convert::tensorToBuffer(std::get<Tensor>(d)));
            else if (std::holds_alternative<cv::Mat>(d)) ins.push_back(
                     onnx_convert::imageToTensor(std::get<cv::Mat>(d), {}, onnx_engine::ElementType::Float32,
                                                 "none", {}, {}, "auto", "keep"));
            else return {false, QStringLiteral("输入必须是张量"), {}};
        }
        std::vector<onnx_engine::TensorBuffer> outs;
        if (!s->run(ins, outs, err)) return {false, QString::fromStdString(err), {}};
        ExecResult r;
        for (auto& b : outs) r.outputs.push_back(onnx_convert::tensorToNodeData(b));
        return r;
    }
};
```
> 说明：进入 ONNX 节点的张量应已由「预处理」节点按模型 dtype/shape 生成，故 executor 默认按原样（必要时 `tensorToNodeData` 保留类型）；若 dtype/shape 不符，`Session::run` 会报错。若输入是 `cv::Mat`（未接预处理），退化为 `none/keep` 的直通转换并在形状不符时报错。

- [ ] **Step 3: 测试**（`tests/test_onnx_nodes.cpp`）
  - `onnxNodePortsFromMetadata`：构造 `OnnxInferNode`，`setModelPath(add_dynamic.onnx)` → 断言 `getInPorts().size()==2`、`getOutPorts().size()==1`，端口名 `A/B/C`，类型 `Tensor`。
  - `onnxExecutorRunsAdd`：直接构造执行器，params `{modelPath: add.onnx}`，inputs 两个 `Tensorvia::Tensor`（float32 [3,4]）→ 断言输出为 `Tensor` 且首值为 5。
- [ ] **Step 4: 提交** `git add include/node/OnnxInfer.hpp include/engine/executors/OnnxInferExecutor.hpp tests/test_onnx_nodes.cpp tests/CMakeLists.txt && git commit -m "feat(onnx): inference node and executor"`

---

## Task 6: 后处理任务（YOLO 检测/分割、分类）+ 显示数据

**Files:** Modify `include/engine/tasks/PostProcessRegistry.hpp`（内置任务实现）、`include/node/PostProcess.hpp`；Modify `tests/test_tasks.cpp`

- [ ] **Step 1: `yolo_detect`**（输入 `1×Tensor` → 输出 `1×Image`）
  - 解析输出张量：支持 `[1, 4+nc, N]`（YOLOv8/v11 默认）与 `[1, N, 4+nc]`；取每列最大类别分数 ≥ `conf`；框为 `cx,cy,w,h`（网络输入坐标）→ `xywh2xyxy`；NMS（IoU `iouThr`），最多 `maxBoxes`；在 `networkSize×networkSize` 画布上画框 + `id` + 分数（`id` 用 `std::to_string`，**不假定文本**）。
  - 参数：`networkSize(640)`、`conf(0.25)`、`iou(0.45)`、`maxBoxes(300)`、`drawScore(true)`、`lineWidth(2)`。
- [ ] **Step 2: `yolo_segment`**（输入 `2×Tensor`：检测头 + 原型 → 输出 `1×Image`）
  - 参数：`networkSize(640)`、`conf(0.25)`、`maskThr(0.5)`、`alpha(0.45)`；对每个保留框用掩码系数 × 原型得到掩码，阈值化后半透明叠加并画框。
- [ ] **Step 3: `classify`**（输入 `1×Tensor` → 输出 `1×Number`（Top-1 id））
  - `compute`：softmax 后取 argmax → 输出 `double(id)`；同时通过 `ctx.display({{"top1", id}, {"topk", topk 列表}})` 上报显示数据。
  - 参数：`topk(5)`（仅显示用）。
- [ ] **Step 4: `PostProcessNode`**：同 `PreProcessNode`（`task` 下拉 + `paramDescs` + `setDisplayData` 保存 QVariantMap 供 QML 显示）。
- [ ] **Step 5: 测试**
  - `yoloDetectSynthetic`：构造 1 个已知框（如 `[1,84,1]`，类别 5 分数 0.9，框 `cx=320,cy=320,w=100,h=100`）→ 断言输出 `cv::Mat` 尺寸 640×640 且非空。
  - `classifySynthetic`：logits `[1,5]` 中最大在第 3 类 → 断言输出 `double == 3`；`ctx.display` 捕获到 `top1 == 3`。
- [ ] **Step 6: 提交** `git add include/engine/tasks include/node/PostProcess.hpp tests/test_tasks.cpp && git commit -m "feat(tasks): yolo detect/segment and classification post-processing"`

---

## Task 7: QML、目录与注册

**Files:** Create `qml/node/{TaskNodeControls,OnnxInferNode,PreProcessNode,PostProcessNode}.qml`；Modify `qml/palette/NodeCatalog.qml`、`assets.qrc`、`include/engine/BuiltinExecutors.hpp`

- [ ] **Step 1: `TaskNodeControls.qml`**（可复用：任务下拉 + 参数控件自动生成）
```qml
import QtQuick
import Theme
import Settings

Column {
    id: root
    property var node          // 任务节点（PreProcessNode/PostProcessNode）
    spacing: 6
    width: parent ? parent.width : implicitWidth

    // 任务下拉（沿用前几次菜单的样式）
    Rectangle {
        width: parent.width; height: 24; radius: 5
        color: taskHover.hovered || taskMenu.opened ? Theme.bgHover : Theme.bg
        border.width: 1; border.color: Theme.border
        Text { anchors.centerIn: parent
               text: root.node.taskLabel !== undefined ? root.node.taskLabel : root.node.task
               color: Theme.fg; font.pixelSize: 11
               renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering }
        HoverHandler { id: taskHover }
        TapHandler { onTapped: taskMenu.opened ? taskMenu.close() : taskMenu.open() }
        Menu {
            id: taskMenu; y: -(height + 6)
            background: Rectangle { implicitWidth: 200; color: Theme.bgElev; border.width: 1; border.color: Theme.border; radius: 8 }
            Instantiator {
                model: root.node.taskOptions()
                delegate: MenuItem {
                    required property var modelData
                    text: modelData.name
                    onTriggered: root.node.task = modelData.id
                    contentItem: Text { text: parent.text; color: Theme.fg; font.pixelSize: 11
                                        leftPadding: 12; verticalAlignment: Text.AlignVCenter
                                        renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering }
                    background: Rectangle { radius: 6; color: parent.hovered ? Theme.bgHover : "transparent" }
                }
                onObjectAdded: (i, o) => taskMenu.insertItem(i, o)
                onObjectRemoved: (i, o) => taskMenu.removeItem(i)
            }
        }
    }

    // 参数：按 ParamDesc 生成
    Repeater {
        model: root.node.paramDescs()
        delegate: Row {
            required property var modelData
            spacing: 6
            Text { width: 60; text: modelData.label; color: Theme.fgDim; font.pixelSize: 11
                   renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering }
            // kind: int/float -> TextInput；bool -> 简易开关；select -> 菜单
            Rectangle {
                width: 90; height: 22; radius: 5; color: Theme.bg; border.width: 1; border.color: Theme.border
                TextInput {
                    anchors.fill: parent; horizontalAlignment: TextInput.AlignHCenter
                    verticalAlignment: TextInput.AlignVCenter; color: Theme.fg; font.pixelSize: 11
                    text: "" + root.node.taskParams()[modelData.key]
                    onEditingFinished: root.node.setTaskParam(modelData.key, text)
                    renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
                }
            }
        }
    }
}
```
> 说明：为控制复杂度，首版 `bool/select` 也用文本输入（分别填 `true/false` 与选项值），后续可换 `SwitchControl`/下拉；`taskLabel` 可选。
- [ ] **Step 2: 三个节点 QML**
  - `OnnxInferNode.qml`：模型行（文件名 + 选择 + 重载）、设备下拉、线程输入、IO 概览（`node.ioText`）、无预览。
  - `PreProcessNode.qml`：`TaskNodeControls { node: root }`，无预览。
  - `PostProcessNode.qml`：`TaskNodeControls { node: root }`；若当前任务输出含 Image → 显示预览；若显示数据含 `topk` → 渲染结果面板（类别 id + Top-K 条形）。
- [ ] **Step 3: 注册**：`NodeCatalog` 增 `OnnxInfer`（`math`）、`PreProcess`/`PostProcess`（`process`）；`assets.qrc` 加三个 QML；`registerBuiltinExecutors()` 注册 `OnnxInfer`/`PreProcess`/`PostProcess`。
- [ ] **Step 4: `cmake --build build -j4 && qmllint <三个 qml> && ctest`**；提交 `git add qml assets.qrc include/engine/BuiltinExecutors.hpp && git commit -m "feat(ui): onnx/pre/post node QML, catalog and executor registration"`

---

## Task 8: 实机验证（yolo11n）

**Files:** 无

- [ ] **Step 1: 搭链**：`图像 → 预处理(图像→张量, 640×640, NCHW/RGB/div255) → ONNX(yolo11n.onnx, CPU) → 后处理(YOLO 检测)`；运行，确认：
  - ONNX 端口按元数据生成（`images`→1 输入、`output0`→1 输出）；
  - 后处理输出 640×640 图像（画框/类别 id/置信度）；
  - 切换后处理任务（分割会影响端口数 → 验证重建端口并断线）；分类任务显示结果面板（用 logits 或对检测输出取 Top-1 验证显示通道）。
- [ ] **Step 2: 失败与边界**：模型路径错误 → 节点报错；未接预处理直接接图像 → 形状不符报错并提示，不崩溃。
- [ ] **Step 3: 记录日志/截图**，提交微调（如有）。

---

## Self-Review 记录

- **Spec 覆盖**：§4/§4.1 适配层（Task 1）；§5.1 ONNX 节点（Task 5）；§5.2/5.3/5.4 任务框架与节点（Task 4/6）；§5.5 显示通道（Task 3/6）；§5.6 目录注册（Task 7）；端口重建（Task 2）；测试（Task 1/2/4/5/6）与实机（Task 8）。
- **占位符**：Task 3/4/6 的实现在计划中给出签名与关键逻辑，具体内部实现由实现者补全（无 `TBD`）。
- **类型一致性**：`PortSpec{name,DataType}`（Task 2）被任务框架复用；`TaskSpec`（Task 4）含 `inputs/outputs/params/compute(ctx,...)`（Task 6 扩展 `compute` 带 `ExecuteContext`）；`onnx_convert::*`（Task 1）被 Task 4/5 复用；`setDisplayData`（Task 3）由 Task 6 节点实现。
- **说明**：`TaskSpec::compute` 统一采用带 `ExecuteContext` 的签名（Task 4 起即如此），分类任务用 `ctx.display` 上报 Top-K。
