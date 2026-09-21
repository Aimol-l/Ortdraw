# Ortdraw ONNX 推理引擎（独立 SDK + 推理算子）— 设计文档

- 日期：2026-09-21
- 状态：待评审
- 范围：封装 ONNX Runtime 为**独立的动态库 SDK**（仅依赖 onnxruntime），并在 Ortdraw 中提供「ONNX 推理」节点与「张量 → 图像」节点，支持**多输入多输出**、按模型元数据自动生成端口与类型映射、每输入端口独立的预处理设置、CPU/CUDA 设备与线程配置。
- 非目标：训练/反向传播；把 ONNX 图展开为 Ortdraw 节点；动态 batch 自动批处理；异步推理；多输出合并为单端口。

## 1. 目标

1. 提供独立的 `onnx_engine` 动态库（不强耦合 Qt / OpenCV / Tensorvia / 应用类型），可脱离主工程单独构建。
2. 「ONNX 推理」节点：一个节点 = 一个模型；载入模型后**按模型输入/输出自动生成端口**（多进多出），端口名 = 模型 IO 名。
3. 端口类型依据**模型元数据**：
   - 输入端口一律 `Any`（可接图像或张量；由执行器按该端口设置转换）；
   - 输出端口按 ONNX 元素类型映射：float/double → `Tensor`；bool → `Bool`；整型/单值 → `Number`；不支持/非张量 → `Any`。
4. 每输入端口独立的预处理：模式（自动/图像/原样）、归一化（无 / `/255` / `(x-mean)/std`）、布局通道（自动/RGB/BGR）、尺寸策略（按模型自动 resize / 保持并报错）。
5. 设备与线程：节点参数 `device = 自动 / CPU / CUDA`、`threads`（0=默认）；会话按 `(模型路径, 设备, 线程)` 缓存复用。
6. 新增「张量 → 图像」节点，自动识别布局/通道/数值范围并转成 `cv::Mat`。
7. onnxruntime 为**硬依赖**：CMake 找不到则构建失败并提示。

## 2. 现状与约束

- 现有执行引擎：`NodeData = variant<monostate, cv::Mat, Tensor(Tensorvia), double, bool>`；端口类型 `Image/Tensor/Number/Bool/Any`；执行器 `NodeExecutor::execute(ctx, params, inputs) -> ExecResult{ok,error,outputs}`，按类型注册到 `NodeRegistry`；节点为 `QQuickItem`，以 `params()/setParams()` 序列化。
- 已安装 `onnxruntime-opt-cuda 1.29.0`（`/usr/lib/libonnxruntime.so`、`/usr/include/onnxruntime/`，含 CUDA provider）。
- 执行器**拿不到节点指针**（只有 `params` + `inputs`）：模型路径、设备、线程、每输入设置必须经 `params` 传递；模型 IO 名称按 `ModelInfo` 的顺序与位置对应，无需序列化元数据。

## 3. 独立库 `onnx_engine`

### 3.1 目录与构建

```
onnx_engine/
  CMakeLists.txt            # 可单独 cmake 构建
  include/onnx_engine/*.hpp # 公开头（不含 Ort/Qt/OpenCV）
  src/*.cpp                 # pimpl 实现，含 Ort::* 细节
```
- 产出 `SHARED` 目标 `onnx_engine`（`libonnx_engine.so`）。
- 主工程 `add_subdirectory(onnx_engine)` 并链接。
- 依赖探测：
  - Linux：`find_library(onnxruntime)` + 头 `/usr/include/onnxruntime`（可 `ONNXRUNTIME_ROOT` 覆盖）；
  - Windows：沿用现有 `ONNX_DIR`（`include/` + `lib/`）。
  - 找不到 → `message(FATAL_ERROR ...)` 使构建失败（硬依赖）。
- 公开头**不得**包含 `<onnxruntime/*>`；`Ort::Env`/`Ort::Session`/`Ort::Value` 全在 pimpl 内。

### 3.2 公开 API（草案）

```cpp
namespace onnx_engine {

enum class ElementType { Float32, Float64, Int32, Int64, UInt8, Int8, Bool, Unknown };
enum class Device { Auto, CPU, CUDA };

// 单个模型 IO
struct TensorInfo {
    std::string name;
    ElementType type = ElementType::Unknown;
    std::vector<int64_t> shape;   // -1 表示动态维
    bool isTensor = true;         // 非张量（sequence/map/optional）为 false
};

struct ModelInfo {
    std::string path;
    std::vector<TensorInfo> inputs;
    std::vector<TensorInfo> outputs;
};

// 中性的张量数据（行主序，raw bytes）
struct TensorBuffer {
    ElementType type = ElementType::Unknown;
    std::vector<int64_t> shape;
    std::vector<std::uint8_t> data;
};

struct SessionOptions {
    Device device = Device::Auto;   // Auto：有 CUDA 则用 CUDA，否则 CPU
    int intraThreads = 0;           // 0 = onnxruntime 默认
};

class Session;   // pimpl

class Runtime {
public:
    static Runtime& instance();

    // 读取模型 IO 元数据（内部缓存；文件变化自动失效重建）
    ModelInfo modelInfo(const std::string& path, const SessionOptions& opts = {});
    // 取得共享会话；失败返回 nullptr 并置 error
    // —— 上层无需管理生命周期：缓存/引用计数/淘汰/失效都在库内完成
    std::shared_ptr<Session> session(const std::string& path, const SessionOptions& opts,
                                     std::string& error);

    // 可选：手动使某模型（其所有 key）失效 / 清空全部缓存
    void reload(const std::string& path);
    void clearCache();

    // 可选：缓存上限（LRU）。默认 maxSessions=4，maxBytes=1GiB（按模型文件大小估算权重）
    void setCacheLimits(int maxSessions, std::size_t maxBytes);

    static bool cudaAvailable();     // 探测 CUDA provider 是否可用
};

class Session {
public:
    const ModelInfo& info() const;
    // inputs 按 info().inputs 顺序；outputs 按 info().outputs 顺序返回
    bool run(const std::vector<TensorBuffer>& inputs,
             std::vector<TensorBuffer>& outputs,
             std::string& error);
};

// 便捷：元素类型名 / 字节大小
const char* elementTypeName(ElementType);
int elementTypeSize(ElementType);   // Bool=1，其余按实际大小
}
```

### 3.3 会话生命周期、缓存与并发（全部在 SDK 内部管理）

**上层使用者只需 `session(path, opts)`；不持有、不卸载、不关心淘汰。**

- **键**：`key = (绝对路径, device, intraThreads)`。同 key → 同一个 `Ort::Session`：
  - **多个 ONNX 节点用同一模型（同 device/threads）共享同一会话，权重只有一份**；
  - 同路径但不同 device/threads → 不同会话（provider/线程配置在会话内，无法共享）。
- **引用计数 + LRU**：
  - 缓存持有强引用（`shared_ptr`），执行器运行期间持有一份 → 该会话在本轮推理结束前**不会被淘汰**；
  - 超过上限（默认 `maxSessions=4` 且 `maxBytes=1GiB`，权重按模型文件大小估算）时，淘汰**最久未使用**且当前无外部引用的会话；
  - 上层无需干预；也可用 `setCacheLimits()` 调整、`clearCache()` 清空。
- **失效/重载**：缓存条目记录 `(mtime, size)`；`session()/modelInfo()` 发现文件已变则重建并替换旧条目；模型被删除则返回加载错误。因此「换文件」对上层透明，节点上的「重载」按钮只是显式触发。
- **淘汰/重建时机**：会话在**首次使用时惰性创建**；`clearCache()` 立即释放无引用条目。
- **并发**：缓存读写与淘汰由库内互斥保护；`Ort::Session::Run` 线程安全，可并发；`run()` 期间不持有缓存锁，仅持有该会话的 `shared_ptr`。
- **设备**：`Device::Auto` → `cudaAvailable()` 为真时注册 CUDA provider，否则 CPU；provider 初始化失败时回退 CPU 并给出日志/错误。
- **失败路径**：模型加载失败、provider 不可用、IO 个数/类型/形状不匹配等，统一通过 `error` 文本返回。

## 4. 应用侧适配层

`include/engine/onnx/` 下（依赖库 + OpenCV + Tensorvia）：
- `OnnxTensorConvert.hpp`：
  - `cv::Mat → TensorBuffer`：按目标 dtype/shape、通道序（BGR→RGB）、归一化、resize（插值）、布局（NCHW/NHWC 自适应）；
  - `TensorBuffer → NodeData`：float/double → `Tensorvia::Tensor`；bool → `bool`；整型单值 → `double`；
  - `Tensorvia::Tensor → TensorBuffer`（张量输入直连时）。
- 与库之间只传 `TensorBuffer`/`ModelInfo`。

## 5. 节点

### 5.1 「ONNX 推理」`OnnxInferNode` + `OnnxInferExecutor`

- 生命周期：节点**只保存参数**（路径/设备/线程/每输入设置），**不持有会话**；会话由 SDK 缓存统一管理。多个节点指向同一模型即自动共享权重；节点上的「重载」按钮调用 `Runtime::reload(path)`，其余无需上层干预。
- 参数（可序列化）：
  ```
  modelPath : string
  device    : "auto" | "cpu" | "cuda"
  threads   : int
  inputs    : { "<模型输入名>": { mode:"auto"|"image"|"raw",
                                   norm:"none"|"div255"|"meanstd",
                                   mean:[...], std:[...],
                                   resize:"auto"|"keep",
                                   channel:"auto"|"rgb"|"bgr" } , ... }
  ```
- 载入模型：调 `Runtime::modelInfo()` → 生成端口。
  - 输入端口：`DataType::Any`，名 = 模型输入名，tooltip 显示 dtype/shape。
  - 输出端口：按元数据映射（见 §1.3），名 = 模型输出名。
  - **换模型/重载**：重建端口；如端口集合变化，断开引用被移除端口的连线（复用现有删边路径）。
- 端口重建机制（需在 `BaseNode` 增加受控 API）：
  - 新增 `void rebuildPorts(QList<PortSpec> ins, QList<PortSpec> outs)`（`PortSpec{name, DataType}`）：清空 `m_input_ports`/`m_output_ports`、删除旧 `Port` 对象、按新规格创建、`emit inputPortsChanged()/outputPortsChanged()`，使 `NodeCard` 重新 `syncPorts()`；
  - 重建前由 `NodeManager` 断开所有挂在**被移除端口**上的边（走现有删边命令路径，保持撤销一致）；
  - 端口集合在**模型载入完成后固定**；运行期不再增删（除非重载模型）。
- 执行（worker 线程）：
  1. `session(modelPath, {device,threads})`；失败 → 节点错误。
  2. 按 `info().inputs` 顺序，对第 i 个输入：取 `inputs[i]`；
     - `cv::Mat` → 按该端口设置转 `TensorBuffer`（目标 dtype/shape 来自 `TensorInfo`；`resize:"keep"` 且尺寸不符 → 报错）；
     - `Tensorvia::Tensor` → 转 `TensorBuffer` 并校验 dtype/shape；
     - `mode:"raw"` 表示不当作图像，按原样/最少处理。
  3. `session->run(...)` → 输出 `TensorBuffer` 列表 → 转 `NodeData`（按输出端口类型）。
- UI（QML `OnnxInferNode.qml`）：模型路径选择/重载；设备下拉；线程输入；**每个输入端口的设置行**（归一化/通道/尺寸，`mean/std` 在多值输入框）；输入/输出端口名与 shape 概览；错误徽标沿用节点错误机制。

### 5.2 「张量 → 图像」`TensorToImageNode` + `TensorToImageExecutor`

- 参数：`layout`（auto/NCHW/NHWC/HW/HWC/HW1）、`channels`（auto/1/3/4）、`scale`（auto / 指定倍率）。
- 规则：自动识别布局与通道；数值范围自适应（`[0,1]` → ×255 并 clamp，否则截断到 `[0,255]`）；`auto` 判断依据维度与通道数（1→灰度、3→BGR、4→BGRA）。

### 5.3 目录/注册

- 节点：`include/node/OnnxInfer.hpp`、`include/node/TensorToImage.hpp`；QML：`qml/node/OnnxInferNode.qml`、`qml/node/TensorToImageNode.qml`。
- 执行器：`include/engine/executors/OnnxInferExecutor.hpp`、`TensorToImageExecutor.hpp`，在 `registerBuiltinExecutors()` 注册。
- `NodeCatalog`：新增 `OnnxInfer`（分类 `math`，图标）与 `TensorToImage`（分类 `process`）。

## 6. 错误处理

- 模型不存在/无法解析、provider 不可用、输入个数/类型/形状不匹配、推理失败 → `ExecResult{ok=false,error}`，节点显示失败（红点 + 队列状态 + 状态栏 Tooltip），并写入日志。
- 载入模型阶段（主线程）失败 → 节点内显示错误文本，不生成端口（或保留空端口）。

## 7. 测试

- 库单测 `test_onnx_engine`（链接 `onnx_engine`）：
  - 夹具 `tests/data/add.onnx`（取自 onnx 官方测试数据 `test_add`，129B，双输入 float32）；
  - `Runtime::modelInfo` 读到 A/B 两个 float32 输入与 1 个输出；
  - `run` 两输入相加结果正确；
  - 缺文件 / dtype 不符 / 输入个数不符 → 返回错误；
  - `cudaAvailable()` 不崩溃（返回值不作断言）。
- 执行器单测（并入 `test_executors` 或新套件）：构造 `OnnxInferExecutor` 的 params（含路径与每输入设置），喂两个 `Tensorvia::Tensor`，校验输出；
- `TensorToImage`：NCHW `[1,3,2,2]` 与 `[1,1,2,2]` 转换的尺寸/通道断言。
- 缓存/生命周期：同一 `(path,device,threads)` 两次 `session()` 返回**同一对象**（`get()==`）；不同 threads 返回不同对象；修改模型文件 mtime 后 `session()` 返回**新对象**；`clearCache()` 后重新加载；`setCacheLimits` 后超限淘汰（可用两个不同模型验证）。
- 实机：用 `test_abs`（单输入）把图像接入 ONNX 节点，验证自动转换、端口生成、输出接「张量 → 图像」显示；再验证换模型重建端口与断线。

## 8. 风险与不改动

- 不改动现有 `.ortdraw` 格式（新增节点即新类型；旧文件不受影响）。
- 不改动执行引擎线程模型；库在 worker 线程被调用，注意会话缓存加锁。
- 风险：
  - 换取模型时端口重建需正确断开旧连线（复用现有删边命令路径），否则悬空连线；
  - `test_add` 等官方夹具随 onnx 包版本变化，夹具应**复制进仓库**而非引用外部路径；
  - CUDA provider 在无 GPU 环境需回退 CPU 并给出清晰错误；
  - 大模型元数据读取会建立会话（首次较慢），缓存后复用。
