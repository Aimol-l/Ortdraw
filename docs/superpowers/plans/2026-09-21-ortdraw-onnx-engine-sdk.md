# `onnx_engine` SDK 实施计划（独立动态库）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 交付一个仅依赖 onnxruntime 的独立动态库 `onnx_engine`：读取 ONNX 模型元数据、按名称/顺序运行推理、进程级会话缓存（LRU + 引用计数 + mtime 失效 + 设备/线程键），对外隐藏 `Ort::*`。

**Architecture:** 子目录 `onnx_engine/` 自带 CMake，产出 `SHARED` 目标 `onnx_engine`；公开头只含中性类型（`ElementType/Device/TensorInfo/ModelInfo/TensorBuffer/SessionOptions`），`Runtime`/`Session` 用 pimpl 封装 `Ort::Env`/`Ort::Session`/`Ort::Value`。onnxruntime 为硬依赖（找不到即 `FATAL_ERROR`）。

**Tech Stack:** C++23、CMake、onnxruntime 1.29（`/usr/lib/libonnxruntime.so`、`/usr/include/onnxruntime`）、Qt6 Test（测试在 `tests/`）。

**设计文档：** `docs/superpowers/specs/2026-09-21-ortdraw-onnx-engine-design.md`（§3、§4.1）

**通用命令：**
- 主工程构建：`cmake --build build -j4`
- 测试：`ctest --test-dir build --output-on-failure`
- 单测跑：`./bin/test_onnx_engine`

---

## 文件结构

- Create: `onnx_engine/CMakeLists.txt`
- Create: `onnx_engine/include/onnx_engine/types.hpp`（中性类型）
- Create: `onnx_engine/include/onnx_engine/runtime.hpp`（`Runtime`/`Session` 声明）
- Create: `onnx_engine/src/impl.hpp`（pimpl 定义）
- Create: `onnx_engine/src/runtime.cpp`
- Create: `tests/data/add.onnx`（夹具，来自 onnx 官方 `test_add`）
- Create: `tests/test_onnx_engine.cpp`
- Modify: `CMakeLists.txt`（`add_subdirectory(onnx_engine)` + 主目标链接）
- Modify: `tests/CMakeLists.txt`（`test_onnx_engine` 链接 `onnx_engine`）

---

## Task 1: 子目录、硬依赖与编译骨架

**Files:**
- Create: `onnx_engine/CMakeLists.txt`
- Create: `onnx_engine/include/onnx_engine/types.hpp`
- Create: `onnx_engine/include/onnx_engine/runtime.hpp`
- Create: `onnx_engine/src/impl.hpp`
- Create: `onnx_engine/src/runtime.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: 写 `onnx_engine/include/onnx_engine/types.hpp`**

```cpp
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace onnx_engine {

// 覆盖 ONNX TensorProto 常用元素类型
enum class ElementType {
    Float32, Float64, Float16, BFloat16,
    Int8, Int16, Int32, Int64,
    UInt8, UInt16, UInt32, UInt64,
    Bool, Unknown
};

enum class Device { Auto, CPU, CUDA };

// 单个模型 IO
struct TensorInfo {
    std::string name;
    ElementType type = ElementType::Unknown;
    std::vector<int64_t> shape;   // -1 表示动态维（本 SDK 仅支持静态，见 modelInfo 校验）
    bool isTensor = true;         // 非张量（sequence/map/optional）为 false
};

struct ModelInfo {
    std::string path;
    std::vector<TensorInfo> inputs;
    std::vector<TensorInfo> outputs;
};

// 中性张量数据（行主序，raw bytes）
struct TensorBuffer {
    ElementType type = ElementType::Unknown;
    std::vector<int64_t> shape;
    std::vector<std::uint8_t> data;
};

struct SessionOptions {
    Device device = Device::Auto;   // Auto：有 CUDA 则用 CUDA，否则 CPU
    int intraThreads = 0;           // 0 = onnxruntime 默认
};

const char* elementTypeName(ElementType t);

// 返回元素字节大小；Unknown 返回 0
int elementTypeSize(ElementType t);

} // namespace onnx_engine
```

- [ ] **Step 2: 写 `onnx_engine/include/onnx_engine/runtime.hpp`**

```cpp
#pragma once
#include <memory>
#include <string>
#include "onnx_engine/types.hpp"

namespace onnx_engine {

class Session;   // pimpl

class Session {
public:
    virtual ~Session() = default;
    virtual const ModelInfo& info() const = 0;
    // inputs 按 info().inputs 顺序；outputs 按 info().outputs 顺序返回
    virtual bool run(const std::vector<TensorBuffer>& inputs,
                     std::vector<TensorBuffer>& outputs,
                     std::string& error) = 0;
};

class Runtime {
public:
    static Runtime& instance();

    // 读取模型 IO 元数据（内部缓存；文件变化自动失效重建）
    ModelInfo modelInfo(const std::string& path, const SessionOptions& opts = {});
    // 取得共享会话；失败返回 nullptr 并置 error
    std::shared_ptr<Session> session(const std::string& path, const SessionOptions& opts,
                                     std::string& error);

    void reload(const std::string& path);
    void clearCache();
    void setCacheLimits(int maxSessions, std::size_t maxBytes);

    static bool cudaAvailable();

    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;

private:
    Runtime();
    ~Runtime();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace onnx_engine
```

- [ ] **Step 3: 写 `onnx_engine/CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.30)
project(onnx_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- onnxruntime 探测（硬依赖）----
if(NOT ONNXRUNTIME_ROOT)
    set(ONNXRUNTIME_ROOT "/usr" CACHE PATH "onnxruntime 安装前缀")
endif()
find_library(ONNXRUNTIME_LIB NAMES onnxruntime
    PATHS "${ONNXRUNTIME_ROOT}/lib" "${ONNXRUNTIME_ROOT}/lib64" NO_DEFAULT_PATH)
find_path(ONNXRUNTIME_INCLUDE_DIR onnxruntime_cxx_api.h
    PATHS "${ONNXRUNTIME_ROOT}/include/onnxruntime" NO_DEFAULT_PATH)
if(NOT ONNXRUNTIME_LIB OR NOT ONNXRUNTIME_INCLUDE_DIR)
    message(FATAL_ERROR
        "未找到 onnxruntime。请安装（如 pacman -S onnxruntime-opt）或用 -DONNXRUNTIME_ROOT=<prefix> 指定。")
endif()
message(STATUS "onnxruntime: ${ONNXRUNTIME_LIB}")

add_library(onnx_engine SHARED
    src/runtime.cpp
)
target_include_directories(onnx_engine PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${ONNXRUNTIME_INCLUDE_DIR}"
)
target_link_libraries(onnx_engine PRIVATE "${ONNXRUNTIME_LIB}")
set_target_properties(onnx_engine PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
)
```

- [ ] **Step 4: 写 `onnx_engine/src/impl.hpp`（pimpl）**

```cpp
#pragma once
#include <list>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <onnxruntime_cxx_api.h>
#include "onnx_engine/runtime.hpp"

namespace onnx_engine {

struct SessionImpl : Session {
    Ort::Session ort{nullptr};
    ModelInfo info;
    std::string path;
    std::int64_t mtime = 0;
    std::uint64_t size = 0;
    std::size_t weightBytes = 0;   // 估算权重（= 模型文件大小）

    explicit SessionImpl(std::shared_ptr<Ort::Env> env);
    const ModelInfo& info_() const;
    const ModelInfo& info() const override { return info; }
    bool run(const std::vector<TensorBuffer>& inputs,
             std::vector<TensorBuffer>& outputs, std::string& error) override;
};

struct Runtime::Impl {
    std::shared_ptr<Ort::Env> env;
    std::mutex mutex;
    int maxSessions = 4;
    std::size_t maxBytes = std::size_t(1) << 30;  // 1 GiB
    // key -> session；LRU 用 list 记录使用顺序
    std::unordered_map<std::string, std::shared_ptr<SessionImpl>> sessions;
    std::list<std::string> lru;   // front = 最近使用

    std::string makeKey(const std::string& path, const SessionOptions& o) const;
    void touch(const std::string& key);
    void evictIfNeeded();
    std::shared_ptr<SessionImpl> get(const std::string& path, const SessionOptions& o,
                                     std::string& error);
    void reloadPath(const std::string& path);
};

} // namespace onnx_engine
```

- [ ] **Step 5: 写 `onnx_engine/src/runtime.cpp` 骨架**

> 本任务只实现 `Runtime` 的构造/析构、`elementTypeName/size`、`cudaAvailable()` 与 `Impl` 的键/LRU/淘汰骨架；`get()` 里本任务先返回 `nullptr` 并 `error = "not implemented"`。后续任务补全模型加载与 run。

```cpp
#include "impl.hpp"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace onnx_engine {

const char* elementTypeName(ElementType t) {
    switch (t) {
    case ElementType::Float32:  return "float32";
    case ElementType::Float64:  return "float64";
    case ElementType::Float16:  return "float16";
    case ElementType::BFloat16: return "bfloat16";
    case ElementType::Int8:     return "int8";
    case ElementType::Int16:    return "int16";
    case ElementType::Int32:    return "int32";
    case ElementType::Int64:    return "int64";
    case ElementType::UInt8:    return "uint8";
    case ElementType::UInt16:   return "uint16";
    case ElementType::UInt32:   return "uint32";
    case ElementType::UInt64:   return "uint64";
    case ElementType::Bool:     return "bool";
    default:                    return "unknown";
    }
}

int elementTypeSize(ElementType t) {
    switch (t) {
    case ElementType::Float32:  return 4;
    case ElementType::Float64:  return 8;
    case ElementType::Float16:  return 2;
    case ElementType::BFloat16: return 2;
    case ElementType::Int8:     return 1;
    case ElementType::Int16:    return 2;
    case ElementType::Int32:    return 4;
    case ElementType::Int64:    return 8;
    case ElementType::UInt8:    return 1;
    case ElementType::UInt16:   return 2;
    case ElementType::UInt32:   return 4;
    case ElementType::UInt64:   return 8;
    case ElementType::Bool:     return 1;
    default:                    return 0;
    }
}

std::string Runtime::Impl::makeKey(const std::string& path, const SessionOptions& o) const {
    return path + "|" + std::to_string(int(o.device)) + "|" + std::to_string(o.intraThreads);
}

void Runtime::Impl::touch(const std::string& key) {
    lru.remove(key);
    lru.push_front(key);
}

void Runtime::Impl::evictIfNeeded() {
    std::size_t total = 0;
    for (auto& [k, s] : sessions) total += s->weightBytes;
    while (sessions.size() > std::size_t(std::max(1, maxSessions)) || total > maxBytes) {
        if (sessions.size() <= 1) break;
        // 从 LRU 末尾找 use_count == 1（仅缓存持有）的条目淘汰
        bool removed = false;
        for (auto it = lru.rbegin(); it != lru.rend(); ++it) {
            auto sit = sessions.find(*it);
            if (sit == sessions.end()) continue;
            if (sit->second.use_count() > 1) continue;   // 外部正在使用
            total -= sit->second->weightBytes;
            sessions.erase(sit);
            lru.erase(std::next(it).base());
            removed = true;
            break;
        }
        if (!removed) break;
    }
}

std::shared_ptr<SessionImpl> Runtime::Impl::get(const std::string&, const SessionOptions&,
                                                std::string& error) {
    error = "onnx_engine: not implemented";
    return nullptr;
}

void Runtime::Impl::reloadPath(const std::string& path) {
    for (auto it = sessions.begin(); it != sessions.end();) {
        if (it->second->path == path) {
            lru.remove(it->first);
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

Runtime::Runtime() : m_impl(std::make_unique<Impl>()) {
    m_impl->env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ortdraw");
}
Runtime::~Runtime() = default;

Runtime& Runtime::instance() { static Runtime r; return r; }

ModelInfo Runtime::modelInfo(const std::string&, const SessionOptions&) { return {}; }

std::shared_ptr<Session> Runtime::session(const std::string& path, const SessionOptions& opts,
                                          std::string& error) {
    return m_impl->get(path, opts, error);
}

void Runtime::reload(const std::string& path) {
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    m_impl->reloadPath(path);
}
void Runtime::clearCache() {
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    m_impl->sessions.clear();
    m_impl->lru.clear();
}
void Runtime::setCacheLimits(int maxSessions, std::size_t maxBytes) {
    std::lock_guard<std::mutex> lk(m_impl->mutex);
    m_impl->maxSessions = maxSessions;
    m_impl->maxBytes = maxBytes;
}

bool Runtime::cudaAvailable() {
    try {
        auto providers = Ort::GetAvailableProviders();
        for (const auto& p : providers)
            if (p.find("CUDA") != std::string::npos) return true;
    } catch (...) {}
    return false;
}

} // namespace onnx_engine
```

- [ ] **Step 6: 主工程接入（Modify `CMakeLists.txt`）**

在 `enable_testing()` 之前、`qt_add_executable` 之后加入：

```cmake
add_subdirectory(onnx_engine)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnx_engine)
```

并把 `onnx_engine/include` 加进测试目标（见 Task 2）。

- [ ] **Step 7: 构建验证**

Run: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j4 2>&1 | tail -5`
Expected: `onnx_engine` 编译并链接产出 `libonnx_engine.so`；主程序链接通过。

- [ ] **Step 8: 提交**

```bash
git add onnx_engine CMakeLists.txt
git commit -m "feat(onnx_engine): standalone shared lib skeleton with ort detection"
```

---

## Task 2: 模型元数据 `modelInfo`（含静态维校验）

**Files:**
- Modify: `onnx_engine/src/runtime.cpp`（实现 `SessionImpl` 加载 + `Impl::get` + `Runtime::modelInfo`）
- Create: `tests/data/add.onnx`
- Create: `tests/test_onnx_engine.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 放置夹具**

```bash
mkdir -p tests/data
cp /home/aimol/.local/lib/python3.13/site-packages/onnx/backend/test/data/node/test_add/model.onnx tests/data/add.onnx
ls -la tests/data/add.onnx
```

- [ ] **Step 2: 链接测试目标（Modify `tests/CMakeLists.txt`）**

在 `foreach(src ${TEST_SOURCES})` 的 `target_link_libraries` 中加入 `onnx_engine`，并为 `test_onnx_engine` 传入夹具路径宏：

```cmake
    target_link_libraries(${name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test
        ${OpenCV_LIBRARIES}
        Tensorvia::tensorvia
        onnx_engine
    )
    target_compile_definitions(${name} PRIVATE
        ORTDRAW_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
    )
```

- [ ] **Step 3: 写失败测试 `tests/test_onnx_engine.cpp`**

```cpp
#include <QtTest>
#include "onnx_engine/runtime.hpp"

using namespace onnx_engine;

static std::string fixture() {
    return std::string(ORTDRAW_TEST_DATA_DIR) + "/add.onnx";
}

class TestOnnxEngine : public QObject {
    Q_OBJECT
private slots:
    void modelInfoReadsTwoInputsOneOutput() {
        auto info = Runtime::instance().modelInfo(fixture());
        QCOMPARE(info.inputs.size(), std::size_t(2));
        QCOMPARE(info.outputs.size(), std::size_t(1));
        QCOMPARE(QString::fromStdString(info.inputs[0].type == ElementType::Float32 ? "f32" : "?"),
                 QString("f32"));
        QVERIFY(info.inputs[0].isTensor);
        for (const auto& t : info.inputs)
            for (int64_t d : t.shape) QVERIFY(d > 0);   // 静态维
    }

    void missingFileReturnsError() {
        std::string err;
        auto s = Runtime::instance().session("/no/such/model.onnx", {}, err);
        QVERIFY(!s);
        QVERIFY(!err.empty());
    }
};

QTEST_MAIN(TestOnnxEngine)
#include "test_onnx_engine.moc"
```

- [ ] **Step 4: 运行确认失败**

Run: `cmake -S . -B build && cmake --build build -j4 2>&1 | tail -5`
Expected: `modelInfoReadsTwoInputsOneOutput` 失败（`inputs.size()==0`）。

- [ ] **Step 5: 实现加载与元数据**

在 `onnx_engine/src/runtime.cpp` 中补 `SessionImpl` 构造、`Impl::get`、`Runtime::modelInfo`，并新增内部工具：

```cpp
namespace {

ElementType mapElement(const std::string& s) {
    if (s == "tensor(float)")    return ElementType::Float32;
    if (s == "tensor(double)")   return ElementType::Float64;
    if (s == "tensor(float16)")  return ElementType::Float16;
    if (s == "tensor(bfloat16)") return ElementType::BFloat16;
    if (s == "tensor(int8)")     return ElementType::Int8;
    if (s == "tensor(int16)")    return ElementType::Int16;
    if (s == "tensor(int32)")    return ElementType::Int32;
    if (s == "tensor(int64)")    return ElementType::Int64;
    if (s == "tensor(uint8)")    return ElementType::UInt8;
    if (s == "tensor(uint16)")   return ElementType::UInt16;
    if (s == "tensor(uint32)")   return ElementType::UInt32;
    if (s == "tensor(uint64)")   return ElementType::UInt64;
    if (s == "tensor(bool)")     return ElementType::Bool;
    return ElementType::Unknown;
}

// 静态维校验：所有 dim > 0
bool allStatic(const std::vector<int64_t>& shape) {
    for (int64_t d : shape) if (d <= 0) return false;
    return true;
}

} // namespace
```

`SessionImpl` 构造（host 侧复制 IO 名，避免悬垂指针）：

```cpp
SessionImpl::SessionImpl(std::shared_ptr<Ort::Env> e) : ort(nullptr) {
    Ort::SessionOptions so;
    // 设备与线程在 makeSession 中设置；此处仅占位
    ort = Ort::Session(*e, L"", so);   // 由 makeSession 覆盖
}
```

> 实际实现用一个自由函数 `makeSession(path, opts, mtime, size)` 创建 `SessionImpl`，内部：
> - 按 `opts.device`/`cudaAvailable()` 追加 `CUDAExecutionProvider` 或 `CPUExecutionProvider`，设置 `SetIntraOpNumThreads`；
> - `Ort::Session(env, path, so)`；
> - `GetInputCount/GetOutputCount` + `GetInputNameAllocated/GetInputTypeInfo`，用 `Ort::TypeInfo` 判定 `ONNX_TYPE_TENSOR` 与元素类型，`GetTensorTypeAndShapeInfo().GetShape()`；
> - 任一 shape 非静态 → 抛错（由 `Impl::get` 转成 `error = "暂不支持动态维度"`）；
> - 记录 `mtime/size/weightBytes`。

`Impl::get`：

```cpp
std::shared_ptr<SessionImpl> Runtime::Impl::get(const std::string& path,
                                                const SessionOptions& o, std::string& error) {
    std::error_code ec;
    const auto abs = std::filesystem::absolute(path, ec).string();
    const auto mt = std::filesystem::last_write_time(abs, ec);
    const auto sz = std::filesystem::file_size(abs, ec);
    if (ec) { error = "模型文件不存在: " + path; return nullptr; }
    const auto key = makeKey(abs, o) + "|" + std::to_string(mt.time_since_epoch().count());
    std::lock_guard<std::mutex> lk(mutex);
    auto it = sessions.find(key);
    if (it != sessions.end()) { touch(key); return it->second; }
    error.clear();
    try {
        auto s = makeSession(env, abs, o, std::int64_t(mt.time_since_epoch().count()),
                             std::uint64_t(sz));
        sessions.emplace(key, s);
        touch(key);
        evictIfNeeded();
        return s;
    } catch (const std::exception& e) {
        error = e.what();
        return nullptr;
    }
}
```

`Runtime::modelInfo` 复用 `get()`：

```cpp
ModelInfo Runtime::modelInfo(const std::string& path, const SessionOptions& opts) {
    std::string err;
    auto s = session(path, opts, err);
    return s ? s->info() : ModelInfo{};
}
```

- [ ] **Step 6: 运行确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build -R test_onnx_engine --output-on-failure`
Expected: 2 个用例通过。

- [ ] **Step 7: 提交**

```bash
git add onnx_engine/src/runtime.cpp tests/data/add.onnx tests/test_onnx_engine.cpp tests/CMakeLists.txt
git commit -m "feat(onnx_engine): model metadata (static-only) and session loading"
```

---

## Task 3: `Session::run` 与 `TensorBuffer`

**Files:**
- Modify: `onnx_engine/src/runtime.cpp`
- Modify: `tests/test_onnx_engine.cpp`

- [ ] **Step 1: 写失败测试（追加到 `TestOnnxEngine`）**

```cpp
    void runAdd() {
        std::string err;
        auto s = Runtime::instance().session(fixture(), {}, err);
        QVERIFY2(s, err.c_str());

        auto mk = [](float v) {
            TensorBuffer b; b.type = ElementType::Float32; b.shape = {1};
            b.data.resize(4); std::memcpy(b.data.data(), &v, 4); return b;
        };
        std::vector<TensorBuffer> ins{mk(2.0f), mk(3.0f)}, outs;
        QVERIFY2(s->run(ins, outs, err), err.c_str());
        QCOMPARE(outs.size(), std::size_t(1));
        QCOMPARE(outs[0].type, ElementType::Float32);
        QCOMPARE(outs[0].shape, std::vector<int64_t>{1});
        QCOMPARE(outs[0].data.size(), std::size_t(4));
        float got = 0; std::memcpy(&got, outs[0].data.data(), 4);
        QCOMPARE(got, 5.0f);
    }

    void runWrongInputCountFails() {
        std::string err;
        auto s = Runtime::instance().session(fixture(), {}, err);
        QVERIFY(s);
        TensorBuffer one; one.type = ElementType::Float32; one.shape = {1}; one.data.resize(4);
        std::vector<TensorBuffer> outs;
        QVERIFY(!s->run({one}, outs, err));
        QVERIFY(!err.empty());
    }
```

- [ ] **Step 2: 运行确认失败**

Run: `cmake --build build -j4 && ./bin/test_onnx_engine`
Expected: `runAdd` 失败（`run` 未实现）。

- [ ] **Step 3: 实现 `SessionImpl::run`**

```cpp
bool SessionImpl::run(const std::vector<TensorBuffer>& inputs,
                      std::vector<TensorBuffer>& outputs, std::string& error) {
    if (inputs.size() != info.inputs.size()) {
        error = "输入个数不符：期望 " + std::to_string(info.inputs.size())
              + "，实际 " + std::to_string(inputs.size());
        return false;
    }
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> in;
        std::vector<std::string> inNames, outNames;
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const auto& b = inputs[i];
            const auto& want = info.inputs[i];
            if (b.type != want.type) {
                error = "输入 " + want.name + " 类型不符：" + std::string(elementTypeName(b.type))
                      + " != " + std::string(elementTypeName(want.type));
                return false;
            }
            if (b.shape != want.shape) {
                error = "输入 " + want.name + " 形状不符";
                return false;
            }
            const std::size_t bytes = b.data.size();
            in.emplace_back(Ort::Value::CreateTensor(mem, const_cast<std::uint8_t*>(b.data.data()),
                                                     bytes, b.shape.data(), b.shape.size(),
                                                     ortType(b.type)));
            inNames.push_back(want.name);
        }
        for (const auto& o : info.outputs) outNames.push_back(o.name);
        std::vector<const char*> ip, op;
        for (auto& n : inNames) ip.push_back(n.c_str());
        for (auto& n : outNames) op.push_back(n.c_str());

        auto res = ort.Run(Ort::RunOptions{nullptr},
                           ip.data(), in.data(), in.size(),
                           op.data(), op.size());
        outputs.clear();
        for (std::size_t i = 0; i < res.size(); ++i) {
            auto ti = res[i].GetTensorTypeAndShapeInfo();
            TensorBuffer b;
            b.type = unmapElement(ti.GetElementType());
            b.shape = ti.GetShape();
            const std::size_t bytes = ti.GetElementCount() * std::size_t(elementTypeSize(b.type));
            const auto* p = res[i].GetTensorData<std::uint8_t>();
            b.data.assign(p, p + bytes);
            outputs.push_back(std::move(b));
        }
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
```

> 另需实现 `ortType(ElementType)`/`unmapElement(ONNXTensorElementDataType)` 两个静态映射函数（与 `mapElement` 表对应）。

- [ ] **Step 4: 运行确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build -R test_onnx_engine --output-on-failure`
Expected: 全部通过（含 `runAdd`、`runWrongInputCountFails`）。

- [ ] **Step 5: 提交**

```bash
git add onnx_engine/src/runtime.cpp tests/test_onnx_engine.cpp
git commit -m "feat(onnx_engine): tensor run with dtype/shape validation"
```

---

## Task 4: 设备与线程（CPU/CUDA、`cudaAvailable`）

**Files:**
- Modify: `onnx_engine/src/runtime.cpp`
- Modify: `tests/test_onnx_engine.cpp`

- [ ] **Step 1: 写测试**

```cpp
    void cpuDeviceRunsAdd() {
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 1;
        auto s = Runtime::instance().session(fixture(), o, err);
        QVERIFY2(s, err.c_str());
    }

    void cudaProbeDoesNotCrash() {
        (void)Runtime::cudaAvailable();   // 仅要求不崩溃
    }
```

- [ ] **Step 2: 实现 provider/线程设置**

在 `makeSession` 内：

```cpp
    Ort::SessionOptions so;
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    if (opts.intraThreads > 0) so.SetIntraOpNumThreads(opts.intraThreads);
    const bool wantCuda = (opts.device == Device::CUDA)
                       || (opts.device == Device::Auto && Runtime::cudaAvailable());
    if (wantCuda) {
        try {
            OrtCUDAProviderOptions cuda{};
            so.AppendExecutionProvider_CUDA(cuda);
        } catch (const std::exception&) {
            // 回退 CPU，并写入日志（库内可用 fprintf(stderr,...)）
        }
    }
    so.AppendExecutionProvider_CPU(1);
```

> `Device::CUDA` 显式请求但不可用时也要回退 CPU（记录 stderr 提示），不抛异常。

- [ ] **Step 3: 运行 + 提交**

Run: `cmake --build build -j4 && ctest --test-dir build -R test_onnx_engine --output-on-failure`
Expected: 通过。

```bash
git add onnx_engine/src/runtime.cpp tests/test_onnx_engine.cpp
git commit -m "feat(onnx_engine): device/provider and thread options"
```

---

## Task 5: 会话缓存语义（同键复用、LRU 淘汰、mtime 失效、clear/reload）

**Files:**
- Modify: `onnx_engine/src/runtime.cpp`
- Modify: `tests/test_onnx_engine.cpp`

- [ ] **Step 1: 写测试**

```cpp
    void sameKeySharesSession() {
        std::string e1, e2;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 2;
        auto a = Runtime::instance().session(fixture(), o, e1);
        auto b = Runtime::instance().session(fixture(), o, e2);
        QVERIFY(a && b);
        QCOMPARE(a.get(), b.get());          // 同一对象
    }

    void differentThreadsDifferentSession() {
        std::string e1, e2;
        SessionOptions o1; o1.device = Device::CPU; o1.intraThreads = 1;
        SessionOptions o2; o2.device = Device::CPU; o2.intraThreads = 3;
        auto a = Runtime::instance().session(fixture(), o1, e1);
        auto b = Runtime::instance().session(fixture(), o2, e2);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());
    }

    void clearCacheReloads() {
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 5;
        auto a = Runtime::instance().session(fixture(), o, err);
        Runtime::instance().clearCache();
        auto b = Runtime::instance().session(fixture(), o, err);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());         // 清空后重建
    }

    void reloadInvalidates() {
        std::string err;
        SessionOptions o; o.device = Device::CPU; o.intraThreads = 6;
        auto a = Runtime::instance().session(fixture(), o, err);
        Runtime::instance().reload(fixture());
        auto b = Runtime::instance().session(fixture(), o, err);
        QVERIFY(a && b);
        QVERIFY(a.get() != b.get());
    }
```

- [ ] **Step 2: 确认失败/通过**

Run: `cmake --build build -j4 && ./bin/test_onnx_engine`
Expected: `sameKeySharesSession` 通过（键含 mtime，自动复用）；`differentThreadsDifferentSession` 应通过；`clearCacheReloads`/`reloadInvalidates` 通过。若 `sameKeySharesSession` 失败，检查 `makeKey` 是否包含 mtime 且 `get()` 命中缓存。

- [ ] **Step 3: 提交**

```bash
git add tests/test_onnx_engine.cpp onnx_engine/src/runtime.cpp
git commit -m "test(onnx_engine): cache identity, clear/reload semantics"
```

---

## Task 6: 全量构建与实机冒烟

**Files:** 无（验证任务）

- [ ] **Step 1: 全量测试**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 全部通过（17 + `test_onnx_engine`）。

- [ ] **Step 2: 独立构建本库**

Run:
```bash
cmake -S onnx_engine -B /tmp/onnx_engine_build && cmake --build /tmp/onnx_engine_build -j4
ls -la /tmp/onnx_engine_build/libonnx_engine.so
```
Expected: 子目录可脱离主工程单独构建。

- [ ] **Step 3: 提交（如有调整）**

```bash
git add -A && git commit -m "chore(onnx_engine): standalone build verification"
```

---

## Self-Review 记录

- **Spec 覆盖**：§3.1 目录/构建/硬依赖（Task 1）；§3.2 API（Task 1/2/3/4）；§3.3 生命周期/LRU/mtime（Task 5）；§4.1 dtype 映射（Task 2/3 的 `mapElement`/`ortType`）；测试（Task 2-5）。
- **占位符**：无 TBD；每步给出代码或明确命令。Task 2 的实现说明较长（`makeSession` 细节），但给出了关键代码与必测断言。
- **类型一致性**：`ElementType/Device/TensorInfo/ModelInfo/TensorBuffer/SessionOptions` 在 Task 1 定义，后续任务复用；`Runtime::session` 返回 `shared_ptr<Session>`，`SessionImpl : Session`。
- **注意**：Task 3 的 `Run` 重载必须同时传输入名与输出名数组（示例已是正确写法）。
