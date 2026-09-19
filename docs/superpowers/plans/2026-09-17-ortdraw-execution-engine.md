# Ortdraw 执行引擎与节点 SDK — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让节点真正处理图像；提供可扩展的节点执行 SDK（数据模型 + 执行接口 + 注册表 + 结果缓存/预览），为未来 ONNX 推理节点铺路。

**Architecture:** 端口类型收敛为 `Image/Tensor/Number/Bool/Any`；张量复用 **Tensorvia**（`via::Tensor`）；节点执行抽象为 `NodeExecutor` + `NodeRegistry`；`GraphExecutor` 在后台线程按拓扑序求值，结果经信号回主线程写入 `ImageStore`（`QQuickImageProvider`）；QML 预览与 ImageShow 显示真实图像。

**Tech Stack:** C++23、Qt 6.11（Core/Gui/Quick/Widgets/Test）、OpenCV、**Tensorvia**、CMake、Qt Test。

**Spec:** `docs/superpowers/specs/2026-09-17-ortdraw-execution-engine-design.md`

---

## 环境与约定
- 工作目录：`/home/aimol/Documents/C++/Workspace/Ortdraw`
- 构建：`cmake --build build -j4`；测试：`ctest --test-dir build --output-on-failure`；qmllint：`/usr/lib/qt6/bin/qmllint`
- 显示可用：`DISPLAY=:0 ./bin/main [--demo]`；截图 `DISPLAY=:0 import -window <WID>`（`WID` 由 `xwininfo -root -tree | grep '"Ortdraw"'` 取）
- Tensorvia：`find_package(Tensorvia REQUIRED)`，目标 `Tensorvia::tensorvia`，头文件 `<tensorvia/core/tensor.h>`（`via::Tensor`）。
- 现有 9 个测试套件必须保持通过；不提交（除非用户要求）。

---

## Task 1: 端口类型迁移（Number / Any）

**Files:** Modify `include/port/Port.hpp`, `include/utils/DAGraph.hpp`, `tests/test_dagraph.cpp`, `tests/test_port.cpp`.

- [ ] **Step 1: 改枚举与名称**（`include/port/Port.hpp`）
```cpp
enum class DataType{ Image, Tensor, Number, Bool, Any };
```
`dataTypeName()` switch 改为：`Image→"Image"`、`Tensor→"Tensor"`、`Number→"Number"`、`Bool→"Bool"`、`Any→"Any"`，默认 `"?"`。
新增静态兼容函数：
```cpp
static bool compatible(DataType a, DataType b){
    return a == b || a == DataType::Any || b == DataType::Any;
}
```

- [ ] **Step 2: `DAGraph::addEdge` 用兼容规则**：把 `if(src->dataType() != dst->dataType()) return false;` 改为
  `if(!Port::compatible(src->dataType(), dst->dataType())) return false;`（`Port.hpp` 已包含）。

- [ ] **Step 3: 修测试**：`tests/test_dagraph.cpp` 的类型不匹配用例把 `DataType::Float` 改成 `DataType::Tensor`（与 Image 不同）；`tests/test_port.cpp` 里 `DataType::Float` 改成 `DataType::Number`。

- [ ] **Step 4: 新增兼容性测试**（`tests/test_dagraph.cpp`）：`Number` 连 `Number` 通过；`Any` 连 `Image` 通过；`Image` 连 `Tensor` 拒绝。

- [ ] **Step 5: 构建 + 测试**：`cmake --build build -j4 && ctest --test-dir build --output-on-failure` → 9 套件通过。

---

## Task 2: 接入 Tensorvia

**Files:** Modify `CMakeLists.txt`, `tests/CMakeLists.txt`; Create `tests/test_tensorvia.cpp`.

- [ ] **Step 1: CMake**
  - 顶层：`find_package(Tensorvia REQUIRED)`；`main` 目标 `target_link_libraries(... Tensorvia::tensorvia)`。
  - `tests/CMakeLists.txt`：每个测试目标也链接 `Tensorvia::tensorvia`（测试会 include 张量头）。

- [ ] **Step 2: 冒烟测试 `tests/test_tensorvia.cpp`**
```cpp
#include <QtTest>
#include <tensorvia/core/tensor.h>
class TestTensorvia : public QObject {
    Q_OBJECT
private slots:
    void createAndRead() {
        Tensor t = Tensor::Zeros({2, 3}, via::DataType::FLOAT32);
        QCOMPARE(int(t.shape(0)), 2);
        QCOMPARE(int(t.shape(1)), 3);
        QCOMPARE(int(t.numel()), 6);
        QCOMPARE(t.dtype(), via::DataType::FLOAT32);
    }
};
QTEST_MAIN(TestTensorvia)
#include "test_tensorvia.moc"
```
（若 `via::DataType` 名称不同，以实际头文件为准调整。）

- [ ] **Step 3: 构建 + 测试**：`cmake -S . -B build && cmake --build build -j4 && ctest --test-dir build -R test_tensorvia --output-on-failure` → PASS；全量 10 套件通过。

---

## Task 3: 引擎 SDK（数据模型 + 接口 + 注册表）

**Files:** Create `include/engine/NodeData.hpp`, `include/engine/NodeExecutor.hpp`, `include/engine/NodeRegistry.hpp`.

- [ ] **Step 1: `include/engine/NodeData.hpp`**
```cpp
#pragma once
#include <variant>
#include <opencv2/core.hpp>
#include <tensorvia/core/tensor.h>

using Tensor = via::Tensor;
using NodeData = std::variant<std::monostate, cv::Mat, Tensor, double, bool>;

inline QString nodeDataName(const NodeData& d) {
    if (std::holds_alternative<cv::Mat>(d)) return "Image";
    if (std::holds_alternative<Tensor>(d)) return "Tensor";
    if (std::holds_alternative<double>(d)) return "Number";
    if (std::holds_alternative<bool>(d)) return "Bool";
    return "None";
}
```

- [ ] **Step 2: `include/engine/NodeExecutor.hpp`**
```cpp
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
```

- [ ] **Step 3: `include/engine/NodeRegistry.hpp`**
```cpp
#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>
#include "engine/NodeExecutor.hpp"

class NodeRegistry {
public:
    static NodeRegistry& instance() { static NodeRegistry r; return r; }
    void registerExecutor(const QString& type, std::shared_ptr<NodeExecutor> exec) {
        m_map.insert(type, std::move(exec));
    }
    std::shared_ptr<NodeExecutor> executorFor(const QString& type) const {
        return m_map.value(type, nullptr);
    }
    QStringList knownTypes() const { return m_map.keys(); }
private:
    QHash<QString, std::shared_ptr<NodeExecutor>> m_map;
};
```

- [ ] **Step 4: 构建**（这些头尚未被使用；确保 `CMakeLists` 的 include glob `include/engine/*.hpp` 覆盖——若未覆盖，追加该 glob）。

---

## Task 4: 内置执行器 + 测试

**Files:** Create `include/engine/executors/*.hpp`, `include/engine/BuiltinExecutors.hpp`; Create `tests/test_executors.cpp`.

- [ ] **Step 1: 图像互转辅助**（`include/engine/executors/ImageConvert.hpp`）
```cpp
#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/NodeData.hpp"

inline cv::Mat tensorToMat(const Tensor& t) {
    Tensor c = t;
    if (c.device() != via::Device::CPU) c.to_host();
    c = c.contiguous();
    const auto shp = c.shape();
    if (shp.size() != 2) return {};
    int rows = int(shp[0]), cols = int(shp[1]);
    cv::Mat m(rows, cols, CV_32F);
    std::memcpy(m.data, c.data(), size_t(rows) * cols * sizeof(float));
    return m;
}
inline Tensor matToTensor(const cv::Mat& m) {
    cv::Mat f; m.convertTo(f, CV_32F);
    std::vector<float> v(f.begin<float>(), f.end<float>());
    std::vector<int64_t> shape{ f.rows, f.cols };
    return Tensor(v, shape);
}
```

- [ ] **Step 2: 各执行器**（`ImageLoad/ImageShow/Resize/Blur/Threshold/Conv`），实现 `execute`：
  - **ImageLoad**：`path = params["path"].toString()`；空或 `cv::imread` 失败 → `{false, "无法读取图片: <path>"}`；否则输出 `cv::Mat`。
  - **ImageShow**：若 `inputs[0]` 非 Image → 错误；否则透传。
  - **Resize**：输入须 Image；`mode==0` 用 `outWidth/outHeight`，`mode==1` 用 `percent/100 * 原尺寸`；`cv::resize`（线性）。
  - **Blur**：输入 Image；`kernel` 取奇数（≥1）；`cv::GaussianBlur`。
  - **Threshold**：输入 Image；转灰度后 `cv::threshold(thr, 255, THRESH_BINARY)`。
  - **Conv**：输入 Image；若 `inputs` 有 Tensor（`inputs[1]` 或首个 Tensor 输入）→ `tensorToMat` 作核；无核 → 错误「缺少卷积核」；`cv::filter2D`（必要时 `cv::normalize` 或要求核为 FLOAT32）。
  - `include/engine/BuiltinExecutors.hpp`：
    ```cpp
    inline void registerBuiltinExecutors() {
        auto& r = NodeRegistry::instance();
        r.registerExecutor("ImageLoad",  std::make_shared<ImageLoadExecutor>());
        r.registerExecutor("ImageShow",  std::make_shared<ImageShowExecutor>());
        r.registerExecutor("Resize",     std::make_shared<ResizeExecutor>());
        r.registerExecutor("Blur",       std::make_shared<BlurExecutor>());
        r.registerExecutor("Threshold",  std::make_shared<ThresholdExecutor>());
        r.registerExecutor("Conv",       std::make_shared<ConvExecutor>());
    }
    ```

- [ ] **Step 3: 测试 `tests/test_executors.cpp`**（无需 GUI）：构造 `cv::Mat`（如 8x8 灰度），断言：
  - Resize 输出尺寸 == 设定值；Blur 输出尺寸不变且平均值接近；Threshold 输出仅含 0/255；Conv 用恒等核（[[0,0,0],[0,1,0],[0,0,0]]）近似等于原图；ImageLoad 空路径返回 ok=false。
  - 张量核用 `via::Tensor(std::vector<float>{...}, {3,3})` 构造。
- [ ] **Step 4:** `cmake --build build -j4 && ctest -R test_executors --output-on-failure` → PASS；全量通过。

---

## Task 5: GraphExecutor（拓扑 + 后台线程 + 错误传播）+ 测试

**Files:** Create `include/engine/GraphExecutor.hpp`; Modify `include/NodeManager.h`, `src/main.cpp`; Create `tests/test_graphexecutor.cpp`.

- [ ] **Step 1: 拓扑排序 + 执行**：`GraphExecutor` 持有 `DAGraph*`（由 NodeManager 注入）与运行状态。
  - `Q_INVOKABLE bool run()`：若无运行中 → 递增 `run_id`、置 running、`std::thread` 启动后台求值。
  - 后台：由 `getAllEdges()` 构建邻接与入度，Kahn 拓扑排序；对每个节点解析输入（沿入边取缓存值），`NodeRegistry::executorFor(node->typeName())->execute(...)`；无执行器 → 跳过并记错误。
  - 单节点失败 → 记录错误，将其后继标记跳过；其余继续。结果缓存 `QHash<BaseNode*, QVector<NodeData>>`。
  - 每节点完成后用 `QMetaObject::invokeMethod(this, [=]{ emit nodeFinished(uuid, ok, err); }, Qt::QueuedConnection)` 回主线程；结束 `emit runFinished(ok)`。
  - `Q_INVOKABLE void cancel()`：置 `cancel` 标志。
  - Q_PROPERTY：`running`、`status`（QString）+ NOTIFY。
- [ ] **Step 2: NodeManager**：成员 `GraphExecutor* m_executor`，`Q_INVOKABLE void run()`/`cancel()` 转发；`setPaintBoard` 时把 `&m_paint_board->m_graph` 注入执行器；暴露 `graphExecutor()` 供 QML 绑定 `running/status`。
- [ ] **Step 3: `src/main.cpp`**：`registerBuiltinExecutors();`
- [ ] **Step 4: 测试**：用 `DAGraph` 建 `ImageLoad→Resize→ImageShow`（直接 `addNode`/`addEdge`），设置 path 为测试图片（用 `cv::imwrite` 到临时文件），跑执行器，断言拓扑顺序与输出缓存存在、ImageLoad path 无效时下游错误。
  - 注意 `GraphExecutor` 依赖 Qt 事件循环回传信号；测试可用 `QSignalSpy` + `QTRY_VERIFY`。
- [ ] **Step 5:** 构建 + 测试通过。

---

## Task 6: ImageStore 与预览

**Files:** Create `include/engine/ImageStore.hpp`; Modify `src/main.cpp`, `qml/node/NodeCard.qml`, `qml/node/ImageShowNode.qml`（或 NodeCard previewSource 逻辑）。

- [ ] **Step 1: `ImageStore`**（`QQuickImageProvider`）：
```cpp
class ImageStore : public QQuickImageProvider {
public:
    ImageStore() : QQuickImageProvider(QQuickImageProvider::Image) {}
    static ImageStore* instance(){ static ImageStore s; return &s; }
    void setImage(const QString& uuid, const QImage& img){ QMutexLocker l(&m_mutex); m_images[uuid]=img; ++m_rev[uuid]; }
    void clear(){ QMutexLocker l(&m_mutex); m_images.clear(); m_rev.clear(); }
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        QMutexLocker l(&m_mutex);
        const QString key = id.section('?', 0, 0);
        QImage img = m_images.value(key);
        if (size) *size = img.size();
        return img;
    }
    qint64 rev(const QString& uuid) const { QMutexLocker l(&m_mutex); return m_rev.value(uuid, 0); }
private:
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images;
    QHash<QString, qint64> m_rev;
};
```
（`QMutexLocker` 需 `#include <QMutex>`。）
- [ ] **Step 2: 注册提供者**（`src/main.cpp`）：`engine.addImageProvider("nodeimage", ImageStore::instance());`
- [ ] **Step 3: 结果→QImage**：`GraphExecutor::nodeFinished` 或新增信号携带 `QImage`（在 worker 线程把首个 Image 输出转 `QImage`：`cv::Mat`(BGR)→`QImage`(RGB888) 拷贝），主线程槽写入 `ImageStore`。
- [ ] **Step 4: QML 预览**：`NodeCard.previewSource` 默认占位图；当 `ImageStore` 有该节点结果时用 `"image://nodeimage/<uuid>?v=<rev>"`。用一个 `Connections`/属性刷新（rev 变化触发重绑）。
- [ ] **Step 5: 验证**：运行后节点缩略图与 ImageShow 显示真实图像（占位图被替换）；无结果时占位图。

---

## Task 7: ImageLoad 选图 + 运行/状态 UI

**Files:** Modify `include/FileDialogs.h`, `include/node/ImageLoad.hpp`, `qml/node/ImageLoadNode.qml`, `qml/node/NodeCard.qml`, `qml/chrome/TopBar.qml`, `qml/chrome/StatusBar.qml`, `qml/main.qml`.

- [ ] **Step 1: `FileDialogs::openImage()`**：`QFileDialog::getOpenFileName(..., "图片 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)")`。
- [ ] **Step 2: `ImageLoadNode`**：新增 `Q_PROPERTY(QString path ...)` + `params()/setParams()` 覆盖（`{"path": path}`）。
- [ ] **Step 3: `ImageLoadNode.qml`**：在 `NodeCard` 内容槽加「选择图片」按钮 + 显示文件名；点击 `var p = FileDialogs.openImage(); if (p!=="") root.path = p`。
- [ ] **Step 4: TopBar 运行按钮**：启用；`running ? "运行中…" : "运行"`；点击 `NodeManager.run()`（运行中则 `cancel()`）。加 `runRequested()` 信号或直接调用。
- [ ] **Step 5: StatusBar**：显示引擎 `status`（就绪/运行中/完成/失败）。
- [ ] **Step 6: NodeCard 错误标记**：节点执行失败时右上角红点 + tooltip（错误文本）。
- [ ] **Step 7: 验证（GUI）**：选图 → 运行 → 预览与 ImageShow 显示结果；改参数重跑刷新；错误节点红点。

---

## Task 8: 文档与整体校验

- [ ] `README.md`：新增「执行引擎」章节（运行按钮、ImageLoad 选图、真实预览、Tensorvia 依赖、错误标记）；依赖补 `Tensorvia`。
- [ ] `rm -rf build && cmake -S . -B build && cmake --build build -j4`（无警告）+ `ctest --test-dir build --output-on-failure`（≥11 套件）。
- [ ] `qmllint $(find qml -name '*.qml')` 无语法错误。
- [ ] GUI 冒烟：`ImageLoad(选图) → Resize/Blur/Threshold → ImageShow` 全链路运行出图；错误处理（坏路径）红点 + 状态栏。

---

## 自查记录
- **Spec 覆盖**：类型迁移→Task 1；Tensorvia→Task 2；SDK→Task 3；执行器→Task 4；引擎→Task 5；结果/预览→Task 6；ImageLoad/运行/状态→Task 7；文档→Task 8。
- **占位符**：C++/测试给出完整代码；QML 给出接线要点（以现有组件为基准）。
- **一致性**：`DataType{Image,Tensor,Number,Bool,Any}`、`Port::compatible`、`NodeData=variant<monostate,cv::Mat,via::Tensor,double,bool>`、`NodeExecutor/NodeRegistry/GraphExecutor/ImageStore` 命名在各任务一致。
