# Ortdraw 执行引擎与节点 SDK — 设计文档

- 日期：2026-09-17
- 状态：待评审
- 范围：让节点真正处理图像；提供可扩展的**节点执行 SDK**（数据模型 + 执行接口 + 注册表 + 结果缓存/预览），为后续 ONNX 推理节点铺路。
- 非目标：ONNX Runtime 接入（后续基于本 SDK 实现）；自动重算；GPU；多画布。

## 1. 目标

1. `ImageLoad` 能选择图片并读入（原生文件对话框），产出图像数据。
2. `Resize / Blur / Threshold / Conv` 用 OpenCV 真正计算并输出图像。
3. `ImageShow` 与所有节点的预览缩略图显示**真实结果**（替换占位图）。
4. 执行模型：**手动点「运行」+ 后台线程**求值，不阻塞 UI；运行结果按节点缓存并回传 UI。
5. 节点执行以 SDK 形式抽象：新节点（含未来的 ONNX 推理节点）只需实现统一接口并注册即可。

## 2. 数据模型

### 2.1 端口类型（本轮定稿）

端口类型集合：

```cpp
enum class DataType { Image, Tensor, Number, Bool, Any };
```

- **Number**：整数与浮点统一为一种数值类型（运行时用 `double` 承载，按需取整/取浮点）。取代原来的 Int/Float 两种。
- **Any**：通配类型，**可与任意类型端口连接**；运行时的实际载荷是上游产出的真实类型，由执行器自行处理/校验。
- `Image` ↔ `cv::Mat`（BGR 或灰度）；`Tensor` ↔ `TensorData`；`Bool` ↔ `bool`。
- 未来可按需扩展：`Mask`（单通道遮罩）、`String`、`Model`（ONNX 句柄）、`Latent`，本期不加入。

### 2.2 NodeData

张量**不自造**，直接复用已有的 **Tensorvia** 库（`tensorvia 0.2.0`，已通过 pacman 安装）：

```cpp
#include <tensorvia/core/tensor.h>
using Tensor = via::Tensor;   // 多后端张量（CPU/CUDA/SYCL/Vulkan），支持多 dtype

using NodeData = std::variant<std::monostate, cv::Mat, Tensor, double, bool>;
```

- `Tensor` ↔ `via::Tensor`：由 Tensorvia 提供（构造、`shape()/dtype()/numel()/contiguous()/to_host()`、`ops::` 运算；CMake 目标 `Tensorvia::tensorvia`）。
- `cv::Mat` 采用引用计数拷贝，跨线程传递安全（只读共享）。
- `std::monostate` 表示「无数据/未连接」。
- `Any` 端口没有独立的运行时类型，其值就是上述 variant 中的实际类型。
- 张量若在其他 device 上，执行器负责 `to_host()/contiguous()` 后再消费。

### 2.3 类型兼容与端口值

- **连线兼容规则**：`a == b || a == Any || b == Any`。即类型相同，或任一端是 `Any` 即可连接。`DAGraph::addEdge` 的校验按此规则。
- 每个节点执行时，输入 = 各输入端口对应的 `NodeData`；输出 = 各输出端口的结果。
- 端口的 `DataType` 参与运行时校验；`Any` 端口的实际值由执行器按需断言/转换。

## 3. 节点执行 SDK

### 3.1 执行上下文

```cpp
struct ExecuteContext {
    QString nodeUuid;
    std::atomic_bool* cancel = nullptr;   // 支持取消
    std::function<void(const QString&)> log; // 可选日志
};

struct ExecResult {
    bool ok = true;
    QString error;                        // ok=false 时的人类可读原因
    QVector<NodeData> outputs;            // 与输出端口一一对应
};
```

### 3.2 执行器接口

```cpp
class NodeExecutor {
public:
    virtual ~NodeExecutor() = default;
    // 默认参数（用于节点创建时可写入 Settings/params 默认值；可返回空）
    virtual QVariantMap defaultParams() const { return {}; }
    virtual ExecResult execute(const ExecuteContext& ctx,
                               const QVariantMap& params,
                               const QVector<NodeData>& inputs) const = 0;
};
```

- 参数来自节点自身的 `params()`（已有 `QVariantMap params()/setParams()`），因此执行器与 UI 参数天然一致。
- 执行器**无状态、可重入**；实例由注册表持有（单例），可并发调用。

### 3.3 注册表

```cpp
class NodeRegistry {
public:
    static NodeRegistry& instance();
    void registerExecutor(const QString& typeName, std::shared_ptr<NodeExecutor> exec);
    std::shared_ptr<NodeExecutor> executorFor(const QString& typeName) const;
    QStringList knownTypes() const;
};
```

- 启动时注册内置执行器：`ImageLoad/ImageShow/Resize/Blur/Threshold/Conv`。
- 未来 ONNX 节点：新增 `OnnxExecutor` + `NodeRegistry::registerExecutor("OnnxInfer", ...)`，无需改引擎。

### 3.4 内置执行器

| 类型 | 输入 | 输出 | 实现 |
| --- | --- | --- | --- |
| ImageLoad | — | Image | 读取 `params.path`（`cv::imread`），失败报错 |
| ImageShow | Image | Image | 透传（并标记为展示节点） |
| Resize | Image | Image | 按 `mode`：尺寸(`outWidth/outHeight`)或百分比(`percent`)，`cv::resize` |
| Blur | Image | Image | 核大小 `kernel`（奇数），`cv::GaussianBlur` |
| Threshold | Image | Image | `threshold`(0–255)，`cv::threshold(..., THRESH_BINARY)` |
| Conv | Image, Tensor | Image | 用 `via::Tensor` 作卷积核（`to_host().contiguous()` 后取 FLOAT32 数据）→ `cv::Mat` → `cv::filter2D`；无核时报「缺少卷积核」 |

> Conv 的核当前无来源（无 Tensor 输入时会报「缺少卷积核」）。可后续增加「核编辑器」或由上游 Tensor 节点提供。

## 4. 图执行引擎

### 4.1 求值流程

```cpp
class GraphExecutor : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE bool run();           // 触发后台求值；已在运行则忽略
    Q_INVOKABLE void cancel();
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
signals:
    void runningChanged();
    void statusChanged();
    void nodeFinished(const QString& uuid, bool ok, const QString& error);
    void runFinished(bool ok);
};
```

流程：
1. 在**调用线程**（主线程）对 `DAGraph` 做拓扑排序（节点集合、边的依赖），生成求值顺序 `QVector<BaseNode*>`。
2. 用 `QtConcurrent::run`/`QThreadPool` 在后台线程按序执行：对每个节点解析输入（沿入边取上游输出），调用 `NodeRegistry::executorFor(type)` 执行。
3. 结果缓存于本次运行的 `QHash<BaseNode*, QVector<NodeData>>`。
4. 每个节点完成后，把「输出图像」转成 `QImage` 并在主线程通过 `ImageStore` 发布（`QueuedConnection`）。
5. 出错节点：记录错误、停止其后继（或整体失败，见 §4.3）、状态栏提示。
6. 结束：`runFinished(ok)`。

### 4.2 输入解析

- 对节点第 i 个输入端口，在边集合中查找 `stop_port->father()==node && stop_port==inputPorts[i]` 的边；找到则取上游对应输出端口的缓存值。
- 未连接且执行器允许缺省 → 传 `monostate`；执行器自行决定是否报错（如 ImageLoad 无输入、Resize 无图像则报错）。

### 4.3 错误策略

- 单节点失败：该节点标记错误，**其下游**全部标记为「跳过/错误」，其余无关分支继续执行。
- 汇总状态：`runFinished(ok=false)`，状态栏显示首个错误；节点卡片右上角显示错误标记（红色小圆点），属性面板/悬浮显示错误文本。

### 4.4 取消与并发

- 全局 `run_id`；每次 `run()` 递增；后台任务检查 `run_id` 是否仍匹配，不匹配则中止（丢弃旧结果）。
- `cancel()` 置取消标志，当前节点完成后停止。
- 运行期间再次点「运行」→ 忽略（或取消后重跑，v1 选择忽略）。

## 5. 结果与预览

### 5.1 ImageStore（QQuickImageProvider）

```cpp
class ImageStore : public QQuickImageProvider {
public:
    static ImageStore* instance();
    void setImage(const QString& nodeUuid, const QImage& img); // 主线程调用
    QImage requestImage(const QString& id, QSize*, const QSize&) override;
    void clear();
};
```
- 以 `image://nodeimage/<uuid>?v=<rev>` 供 QML 使用；`rev` 每次更新递增以强制刷新。
- `NodeCard` 预览：`previewSource` 改为绑定该 URL（有结果时），无结果为占位图。
- `ImageShow` 节点用同一 URL 显示大图。

### 5.2 缩略图

- 引擎在发布结果时，把图像下采样到预览尺寸（如最长边 256）生成缩略图，减少内存与绘制开销；`ImageStore` 存缩略图，大图仅在 ImageViewer/ImageShow 需要时按需生成。

## 6. UI 集成

- **运行按钮**（TopBar 已有占位）：启用；运行中显示「运行中…」并可取消（再点取消）；结束显示「完成/失败」。
- **ImageLoad 节点**：新增参数 `path`，节点内加「选择图片」按钮（`FileDialogs` 原生对话框，图片过滤器）+ 显示文件名；改动触发脏标记。
- **状态栏**：显示运行状态（就绪/运行中/完成/失败 + 错误摘要）。
- **节点错误标记**：节点卡右上角红点 + tooltip/属性面板显示错误。
- **占位图**：未运行或某节点无结果时保留现有占位图。

## 7. 线程与线程安全

- 后台线程只读写 `cv::Mat`/`NodeData` 与执行器；**不触碰 QML/QObject**。
- 结果通过 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 或信号回到主线程，再写入 `ImageStore` 与节点状态。
- 引擎运行期间允许拖动/编辑 UI；但被编辑的节点结果可能过期（手动运行模型下可接受）。运行中禁止再次运行。

## 8. 文件清单

**新增**
- `include/engine/NodeData.hpp`
- `include/engine/NodeExecutor.hpp`（接口 + ExecuteContext/ExecResult）
- `include/engine/NodeRegistry.hpp`
- `include/engine/GraphExecutor.hpp`
- `include/engine/ImageStore.hpp`
- `include/engine/executors/{ImageLoad,ImageShow,Resize,Blur,Threshold,Conv}Executor.hpp`
- `tests/test_executors.cpp`、`tests/test_graphexecutor.cpp`

**依赖**
- 新增依赖 **Tensorvia**（`find_package(Tensorvia REQUIRED)`，链接 `Tensorvia::tensorvia`，头文件 `<tensorvia/...>`），用于张量类型与运算。

**修改**
- `CMakeLists.txt`（`find_package(Tensorvia REQUIRED)` + 链接 `Tensorvia::tensorvia`）
- `include/node/ImageLoad.hpp`（path 参数 + params 覆盖）
- `include/NodeManager.h`（暴露图给引擎；连接节点参数变更）
- `src/main.cpp`（注册 ImageStore 图片提供者、注册内置执行器）
- `qml/node/NodeCard.qml`（预览绑定真实结果 URL、错误标记）
- `qml/main.qml`（运行按钮接线、状态显示）
- `qml/chrome/TopBar.qml`（运行按钮状态）
- `qml/chrome/StatusBar.qml`（运行状态）
- `qml/node/ImageLoadNode.qml`（选择图片按钮）
- `CMakeLists.txt`、`assets.qrc`、`README.md`

## 9. 测试与验证

- 单元测试（无需 GUI）：
  - 各执行器：构造 `cv::Mat` 输入，断言输出尺寸/类型/取值范围（Resize 尺寸、Blur 平滑、Threshold 二值、Conv 恒等核等于原图）。
  - `GraphExecutor` 的拓扑排序：`ImageLoad→Resize→ImageShow` 链、分叉、多入。
  - 错误传播：ImageLoad 路径无效 → 下游标记错误。
- GUI 冒烟：选图 → 运行 → 预览与 ImageShow 显示真实结果；改参数后重跑刷新；错误节点红点。
- 现有 9 个测试套件保持通过。

## 10. 风险与取舍

- **新增依赖 Tensorvia**：提供张量类型；若其设备设为非 CPU，执行器需 `to_host()/contiguous()`。当前 CPU 后端即可。CMake 需能 `find_package(Tensorvia)`（Arch 包已提供 `TensorviaConfig.cmake`）。
- OpenCV 在 worker 线程使用：`cv::Mat` 自身线程安全（拷贝共享），但需避免并发写同一 Mat；单任务串行执行可规避。
- 大图内存：缩略图 + 按需大图；运行结果在下次运行/清空时释放。
- 运行期间编辑：手动模型下结果可能过期，UI 标注「图已变更，需重新运行」（可选）。
- 未来 ONNX：执行器接口需能承载模型加载与推理会话（`OnnxExecutor` 自行管理会话缓存），本 SDK 不预置 ONNX 依赖。
