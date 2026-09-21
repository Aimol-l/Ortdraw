#include "impl.hpp"
#include <atomic>
#include <dlfcn.h>
#include <format>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <expected>
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace onnx_engine {

namespace {

ElementType mapElementType(ONNXTensorElementDataType t) {
    switch (t) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:   return ElementType::Float32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE:  return ElementType::Float64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return ElementType::Float16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16:return ElementType::BFloat16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8:    return ElementType::Int8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16:   return ElementType::Int16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:   return ElementType::Int32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64:   return ElementType::Int64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:   return ElementType::UInt8;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16:  return ElementType::UInt16;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32:  return ElementType::UInt32;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64:  return ElementType::UInt64;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL:    return ElementType::Bool;
    default:                                    return ElementType::Unknown;
    }
}

ONNXTensorElementDataType ortElementType(ElementType t) {
    switch (t) {
    case ElementType::Float32:  return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    case ElementType::Float64:  return ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
    case ElementType::Float16:  return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
    case ElementType::BFloat16: return ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16;
    case ElementType::Int8:     return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
    case ElementType::Int16:    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
    case ElementType::Int32:    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
    case ElementType::Int64:    return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
    case ElementType::UInt8:    return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
    case ElementType::UInt16:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
    case ElementType::UInt32:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
    case ElementType::UInt64:   return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
    case ElementType::Bool:     return ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
    default:                    return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
    }
}

std::string shapeStr(const std::vector<int64_t>& shape) {
    std::string out;
    for (std::size_t i = 0; i < shape.size(); ++i)
        out += std::format("{}{}", i ? "," : "", shape[i]);
    return std::format("[{}]", out);
}

// Tensorvia 可承载的子集：Bool 经 INT8 承载；UInt8/UInt16+ 与 Unknown 不支持。
bool isSupportedElement(ElementType t) {
    switch (t) {
    case ElementType::Float32:
    case ElementType::Float64:
    case ElementType::Float16:
    case ElementType::BFloat16:
    case ElementType::Int8:
    case ElementType::Int16:
    case ElementType::Int32:
    case ElementType::Int64:
    case ElementType::Bool:
        return true;
    default:
        return false;
    }
}

} // namespace

static std::string normPath(const std::string& path) {
    std::error_code ec;
    auto abs = std::filesystem::absolute(path, ec);
    if (ec) return path;
    auto canon = std::filesystem::weakly_canonical(abs, ec);
    if (ec) return abs.string();
    return canon.string();
}

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

via::DataType toViaDataType(ElementType t) {
    switch (t) {
    case ElementType::Float32:  return via::DataType::FLOAT32;
    case ElementType::Float64:  return via::DataType::FLOAT64;
    case ElementType::Float16:  return via::DataType::FLOAT16;
    case ElementType::BFloat16: return via::DataType::BFLOAT16;
    case ElementType::Int8:     return via::DataType::INT8;
    case ElementType::Int16:    return via::DataType::INT16;
    case ElementType::Int32:    return via::DataType::INT32;
    case ElementType::Int64:    return via::DataType::INT64;
    case ElementType::Bool:     return via::DataType::INT8;    // 0/1 承载
    default:                    return via::DataType::FLOAT32;
    }
}

ElementType fromViaDataType(via::DataType t) {
    switch (t) {
    case via::DataType::FLOAT32:  return ElementType::Float32;
    case via::DataType::FLOAT64:  return ElementType::Float64;
    case via::DataType::FLOAT16:  return ElementType::Float16;
    case via::DataType::BFLOAT16: return ElementType::BFloat16;
    case via::DataType::INT8:     return ElementType::Int8;
    case via::DataType::INT16:    return ElementType::Int16;
    case via::DataType::INT32:    return ElementType::Int32;
    case via::DataType::INT64:    return ElementType::Int64;
    default:                      return ElementType::Unknown;
    }
}

// path 需为已规范化的绝对路径（调用方保证），避免重复 normPath
std::string Runtime::Impl::makeKey(const std::string& path, const SessionOptions& o) const {
    // 归一化线程数：<0 与 0 语义相同（onnxruntime 默认），避免重复建会话
    const int threads = o.intraThreads > 0 ? o.intraThreads : 0;
    return std::format("{}|{}|{}", path, int(o.device), threads);
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

// 某些发行版（例如 Arch 的 onnxruntime-opt-cuda）的 provider 动态库缺少对 libcudnn 的
// DT_NEEDED，内部却保留了未定义的 cudnn 符号，导致 dlopen(provider) 以 undefined symbol 失败
// （如 cudnnGetConvolutionBackwardDataAlgorithm_v7）。在 ORT 加载 provider 之前，把 cuDNN 的
// 分发层与子库以 RTLD_GLOBAL 打开，让这些符号全局可见即可正常加载。
// 注意：这些句柄故意不释放，符号必须一直可见；也不要在此 dlopen provider 试探——裸进程里
// provider 的静态初始化可能直接段错误。
static void exposeCudnnSymbols() {
#ifndef _WIN32
    static std::once_flag once;
    std::call_once(once, [] {
        constexpr const char* kProbe = "cudnnGetConvolutionBackwardDataAlgorithm_v7";
        if (::dlsym(RTLD_DEFAULT, kProbe) != nullptr) return;   // 已经可见（正常安装或 LD_PRELOAD）
        const char* libs[] = {"libcudnn.so.9",  "libcudnn_cnn.so.9", "libcudnn_ops.so.9",
                              "libcudnn_adv.so.9", "libcudnn_graph.so.9",
                              "libcudnn_heuristic.so.9"};
        // RTLD_NODELETE：cudnn 在 libonnxruntime 之后加载，若参与退出期卸载，
        // 其 fini 会先于 ORT 的静态析构执行，导致退出时崩溃；标记为不可卸载即可规避。
        for (const char* n : libs)
            (void)::dlopen(n, RTLD_NOW | RTLD_GLOBAL | RTLD_NODELETE);
    });
#endif
}

// provider 必需的 cuDNN 符号是否能解析（Windows 官方包依赖完整，直接交给 ORT 报错）
static bool cudnnSymbolsResolvable() {
#ifdef _WIN32
    return true;
#else
    exposeCudnnSymbols();
    return ::dlsym(RTLD_DEFAULT, "cudnnGetConvolutionBackwardDataAlgorithm_v7") != nullptr;
#endif
}

// CUDA 可用性在本进程内的实测状态：// CUDA 可用性在本进程内的实测状态：0=未知，1=可用，2=已确认不可用（例如 provider 动态库损坏）。
// 确认失败后不再对后续会话重试，避免每个模型都失败一次并刷屏。
static std::atomic<int> g_cudaState{0};
static std::atomic<bool> g_cudaFallbackReported{false};

static std::shared_ptr<SessionImpl> makeSession(std::shared_ptr<Ort::Env> env,
                                                const std::string& absPath,
                                                const SessionOptions& opts,
                                                std::int64_t mtime, std::uint64_t size,
                                                bool useCuda) {
    Ort::SessionOptions so;
    // ORT 的 W 级提示（如“部分节点回退 CPU”）由异常接管，这里只保留 E 级，保持输出干净
    so.SetLogSeverityLevel(ORT_LOGGING_LEVEL_ERROR);
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    if (opts.intraThreads > 0) so.SetIntraOpNumThreads(opts.intraThreads);
    if (useCuda) {
        exposeCudnnSymbols();   // 修复 provider 缺少 cudnn 依赖的发行版包
        OrtCUDAProviderOptions c{};
        c.device_id = 0;
        c.arena_extend_strategy = 0;   // kNextPowerOfTwo
        // EXHAUSTIVE 搜索显存占用和时间都不可控；Heuristic 对推理足够
        c.cudnn_conv_algo_search = OrtCudnnConvAlgoSearchHeuristic;
        c.do_copy_in_default_stream = 1;
        so.AppendExecutionProvider_CUDA(c);   // 失败则抛异常，由调用方决定回退策略
        g_cudaState.store(1, std::memory_order_relaxed);
    }
    auto s = std::make_shared<SessionImpl>(env);
    s->ort = Ort::Session(*env, std::filesystem::path(absPath).c_str(), so);
    s->info_.path = absPath;
    s->path = absPath;
    s->mtime = mtime;
    s->size = size;
    s->weightBytes = std::size_t(size);   // 估算权重：以模型文件大小近似

    Ort::AllocatorWithDefaultOptions alloc;
    auto readIO = [&](size_t count, bool input, std::vector<TensorInfo>& out) {
        for (size_t i = 0; i < count; ++i) {
            Ort::AllocatedStringPtr n = input ? s->ort.GetInputNameAllocated(i, alloc)
                                              : s->ort.GetOutputNameAllocated(i, alloc);
            Ort::TypeInfo ti = input ? s->ort.GetInputTypeInfo(i) : s->ort.GetOutputTypeInfo(i);
            TensorInfo info;
            info.name = n.get();
            if (ti.GetONNXType() == ONNX_TYPE_TENSOR) {
                Ort::ConstTensorTypeAndShapeInfo sh = ti.GetTensorTypeAndShapeInfo();
                info.type = mapElementType(sh.GetElementType());
                info.shape = sh.GetShape();       // 可能含 -1（动态维）
                info.isTensor = true;
            } else {
                info.isTensor = false;
            }
            out.push_back(std::move(info));
        }
    };
    readIO(s->ort.GetInputCount(), true, s->info_.inputs);
    readIO(s->ort.GetOutputCount(), false, s->info_.outputs);

    // 一次性缓存名字与稳定的 c_str 指针（reserve 后不再修改，指针保持有效）
    auto cacheNames = [](const std::vector<TensorInfo>& io, std::vector<std::string>& names,
                         std::vector<const char*>& ptrs) {
        names.clear();
        names.reserve(io.size());
        for (const TensorInfo& t : io) names.push_back(t.name);
        ptrs.clear();
        ptrs.reserve(names.size());
        for (const std::string& n : names) ptrs.push_back(n.c_str());
    };
    cacheNames(s->info_.inputs, s->inputNames, s->inputNamePtrs);
    cacheNames(s->info_.outputs, s->outputNames, s->outputNamePtrs);
    return s;
}

std::shared_ptr<SessionImpl> Runtime::Impl::get(const std::string& path,
                                                const SessionOptions& o, std::string& error) {
    error.clear();
    std::error_code ec;
    const std::string abs = normPath(path);
    const auto mt = std::filesystem::last_write_time(abs, ec);
    const auto sz = std::filesystem::file_size(abs, ec);
    if (ec) { error = std::format("onnx_engine: 模型文件不存在或不可读: {}", path); return nullptr; }
    const auto mtime = std::int64_t(mt.time_since_epoch().count());
    const auto size = std::uint64_t(sz);
    const auto key = std::format("{}|{}|{}", makeKey(abs, o), mtime, size);

    std::shared_ptr<BuildSlot> slot;
    bool leader = false;
    {
        std::lock_guard<std::mutex> lk(mutex);
        auto it = sessions.find(key);
        if (it != sessions.end()) { touch(key); return it->second; }
        auto bit = building.find(key);
        if (bit != building.end()) {
            slot = bit->second;                 // 已有 leader 在加载，等待其结果
        } else {
            slot = std::make_shared<BuildSlot>();
            slot->future = slot->promise.get_future().share();
            building.emplace(key, slot);
            leader = true;
        }
    }

    if (!leader) {
        try {
            return slot->future.get();
        } catch (const std::exception& e) {
            error = e.what();
            return nullptr;
        }
    }

    // leader 在锁外加载/构图，避免持锁阻塞其他 session()/clearCache() 等调用
    std::shared_ptr<SessionImpl> s;
    std::string buildError;
    try {
        const bool wantCuda = (o.device == Device::CUDA)
                           || (o.device == Device::Auto && Runtime::cudaAvailable()
                               && g_cudaState.load(std::memory_order_relaxed) != 2);
        try {
            s = makeSession(env, abs, o, mtime, size, wantCuda);
        } catch (const std::exception& e) {
            if (o.device == Device::Auto && wantCuda) {
                g_cudaState.store(2, std::memory_order_relaxed);
                if (!g_cudaFallbackReported.exchange(true)) {
                    std::fprintf(stderr, "onnx_engine: CUDA 初始化失败，改用 CPU（原因: %s）\n",
                                 e.what());
                }
                s = makeSession(env, abs, o, mtime, size, false);
            } else {
                throw;   // 显式 CUDA：不静默回退
            }
        }
    } catch (const std::exception& e) {
        buildError = e.what();
        s = nullptr;
    }

    {
        std::lock_guard<std::mutex> lk(mutex);
        // 不变量：凡从 building 移除 key 的路径，都必须已对 promise set_value/set_exception；
        // 若中途抛出（如 emplace/evict 失败），本 guard 兜底设置异常并清理 building，
        // 避免等待者永久阻塞在 future.get()。
        struct PromiseGuard {
            Runtime::Impl* self;
            const std::string* key;
            std::shared_ptr<BuildSlot> slot;
            std::string msg;
            bool done = false;
            ~PromiseGuard() {
                if (done) return;
                try {
                    slot->promise.set_exception(
                        std::make_exception_ptr(std::runtime_error(msg)));
                } catch (...) {}
                self->building.erase(*key);
            }
        } guard{this, &key, slot,
                buildError.empty() ? std::string("onnx_engine: 会话构建失败") : buildError};

        if (s) {
            // 同一路径的旧条目（如文件已变化、仅缓存持有）一并淘汰，避免残留旧权重
            for (auto it = sessions.begin(); it != sessions.end();) {
                if (it->first != key && it->second->path == abs && it->second.use_count() == 1) {
                    lru.remove(it->first);
                    it = sessions.erase(it);
                } else {
                    ++it;
                }
            }
            sessions.emplace(key, s);
            touch(key);
            evictIfNeeded();
            slot->promise.set_value(s);
            guard.done = true;
        } else {
            slot->promise.set_exception(std::make_exception_ptr(std::runtime_error(buildError)));
            guard.done = true;
        }
        building.erase(key);
    }
    if (!s) { error = buildError; return nullptr; }
    return s;
}

void Runtime::Impl::reloadPath(const std::string& path) {
    const std::string key = normPath(path);
    for (auto it = sessions.begin(); it != sessions.end();) {
        if (it->second->path == key) {
            lru.remove(it->first);
            it = sessions.erase(it);
        } else {
            ++it;
        }
    }
}

SessionImpl::SessionImpl(std::shared_ptr<Ort::Env> e) : env(std::move(e)), ort(nullptr) {}

std::expected<std::vector<Tensor>, std::string>
SessionImpl::run(const std::vector<Tensor>& inputs) {
    if (inputs.size() != info_.inputs.size()) {
        return std::unexpected(std::format("onnx_engine: 输入个数不符，期望 {}，实际 {}", info_.inputs.size(), inputs.size()));
    }
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> ortInputs;
        std::vector<Tensor> temps;                  // 物化/搬运副本，须存活到 Run 结束
        std::vector<std::vector<int64_t>> shapes;   // 形状存储，保证 CreateTensor 期间指针有效
        ortInputs.reserve(inputs.size());
        temps.reserve(inputs.size());
        shapes.reserve(inputs.size());

        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const Tensor& t = inputs[i];
            const TensorInfo& want = info_.inputs[i];
            if (!want.isTensor) {
                return std::unexpected(std::format("onnx_engine: 输入 {} 不是张量", want.name));
            }
            if (!isSupportedElement(want.type) || elementTypeSize(want.type) <= 0) {
                return std::unexpected(std::format(
                    "onnx_engine: 输入 {} 的元素类型不受支持（{}）",
                    want.name, elementTypeName(want.type)));
            }
            // dtype 校验：模型类型经 §4.1 映射后必须与张量 dtype 一致
            //（如 Bool 模型要求 INT8 张量）
            const via::DataType wantVia = toViaDataType(want.type);
            if (t.dtype() != wantVia) {
                return std::unexpected(std::format(
                    "onnx_engine: 输入 {} 类型不符，期望 {}，实际 {}",
                    want.name, via::dtype_to_string(wantVia), via::dtype_to_string(t.dtype())));
            }
            // 承载宽度必须一致才能按原始字节喂给 ORT
            if (std::size_t(elementTypeSize(want.type)) != via::calc_dtype_size(t.dtype())) {
                return std::unexpected(std::format(
                    "onnx_engine: 输入 {} 的元素承载宽度与模型不匹配（{}），暂不支持",
                    want.name, elementTypeName(want.type)));
            }
            // 形状校验：秩必须一致；期望维 -1（动态）接受任意，其余必须相等
            const auto span = t.shape();
            std::vector<int64_t> shape(span.begin(), span.end());
            if (shape.size() != want.shape.size()) {
                return std::unexpected(std::format(
                    "onnx_engine: 输入 {} 秩不符，期望 {}，实际 {}",
                    want.name, want.shape.size(), shape.size()));
            }
            std::size_t elems = 1;
            for (std::size_t d = 0; d < shape.size(); ++d) {
                if (want.shape[d] != -1 && shape[d] != want.shape[d]) {
                    return std::unexpected(std::format(
                        "onnx_engine: 输入 {} 形状不符，期望 {}，实际 {}",
                        want.name, shapeStr(want.shape), shapeStr(shape)));
                }
                if (shape[d] < 0) {
                    return std::unexpected(std::format(
                        "onnx_engine: 输入 {} 形状含非法维 {}", want.name, shapeStr(shape)));
                }
                elems *= std::size_t(shape[d]);
            }
            if (elems != t.numel()) {
                return std::unexpected(std::format(
                    "onnx_engine: 输入 {} 元素数与形状不符，期望 {}，实际 {}",
                    want.name, elems, t.numel()));
            }

            // 零拷贝：CPU 且连续时直接引用调用方内存；否则物化一份连续 host 副本
            const void* p = nullptr;
            const std::size_t bytes = elems * std::size_t(elementTypeSize(want.type));
            if (t.is_contiguous() && t.device() == via::Device::CPU) {
                p = t.data();
            } else {
                Tensor tmp = t.contiguous();
                tmp.to_host();
                temps.push_back(std::move(tmp));
                p = temps.back().data();
            }
            shapes.push_back(std::move(shape));
            const std::vector<int64_t>& sh = shapes.back();
            ortInputs.emplace_back(Ort::Value::CreateTensor(
                mem, const_cast<void*>(p), bytes, sh.data(), sh.size(),
                ortElementType(want.type)));
        }

        std::vector<Ort::Value> res =
                    ort.Run(
                    Ort::RunOptions{nullptr},
                    inputNamePtrs.data(),
                    ortInputs.data(),
                    ortInputs.size(),
                    outputNamePtrs.data(),
                    outputNamePtrs.size()
                    );

        if (res.size() != info_.outputs.size()) {
            return std::unexpected(std::format(
                "onnx_engine: 输出个数不符，期望 {}，实际 {}", info_.outputs.size(), res.size()));
        }
        // 先写入局部缓冲，只有全部成功后才返回（失败即 unexpected）。
        std::vector<Tensor> tmpOuts;
        tmpOuts.reserve(res.size());
        for (std::size_t i = 0; i < res.size(); ++i) {
            const TensorInfo& want = info_.outputs[i];
            Ort::Value& v = res[i];
            if (!want.isTensor || !v.IsTensor()) {
                return std::unexpected(std::format(
                    "onnx_engine: 输出 {} 不是张量，暂不支持", want.name));
            }
            Ort::TensorTypeAndShapeInfo ti = v.GetTensorTypeAndShapeInfo();
            const ElementType ot = mapElementType(ti.GetElementType());
            if (!isSupportedElement(ot) || elementTypeSize(ot) == 0) {
                return std::unexpected(std::format(
                    "onnx_engine: 输出 {} 的元素类型不受支持", want.name));
            }
            const std::size_t elems = std::size_t(ti.GetElementCount());
            if (elems == 0) {
                // Tensorvia 不接受零元素张量（构造会抛异常），明确报错优于抛出。
                return std::unexpected(std::format(
                    "onnx_engine: 输出 {} 元素数为 0", want.name));
            }
            std::vector<int64_t> shape = ti.GetShape();
            if (shape.empty()) 
                shape.push_back(1);   // Tensorvia 不接受空 shape；标量按 1 元素承载
            const via::DataType dt = toViaDataType(ot);
            Tensor out(shape, dt, via::Device::CPU);

            const int srcSize = elementTypeSize(ot);
            const int dstSize = int(via::calc_dtype_size(dt));
            const std::uint8_t* src = v.GetTensorData<std::uint8_t>();

            if (srcSize == dstSize) {
                std::memcpy(out.data(), src, elems * std::size_t(srcSize));
            } else {
                return std::unexpected(std::format("onnx_engine: 输出 {} 的类型映射暂不支持", want.name));
            }
            tmpOuts.push_back(std::move(out));
        }
        return tmpOuts;
    } catch (const std::exception& e) {
        return std::unexpected(e.what());
    }
}

Runtime::Runtime() : m_impl(std::make_unique<Impl>()) {
    m_impl->env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ortdraw");
}
Runtime::~Runtime() = default;

Runtime& Runtime::instance() {
    // 故意不析构：会话缓存里的 ORT Session 在退出期销毁时，CUDA provider 可能依赖
    // 已被卸载/析构的 cuDNN 状态而崩溃（且静态析构顺序不可控）。进程退出由内核回收，
    // 需要显式释放请调用 clearCache()。
    static Runtime* r = new Runtime();
    return *r;
}

ModelInfo Runtime::modelInfo(const std::string& path, const SessionOptions& opts) {
    std::string error;
    return modelInfo(path, opts, error);
}

ModelInfo Runtime::modelInfo(const std::string& path, const SessionOptions& opts,
                             std::string& error) {
    auto s = session(path, opts, error);
    return s ? s->info() : ModelInfo{};
}

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
    m_impl->evictIfNeeded();
}

bool Runtime::cudaAvailable() {
    // 实测过失败（如 provider 动态库损坏）后，本进程内一律视为不可用
    if (g_cudaState.load(std::memory_order_relaxed) == 2) return false;
    bool listed = false;
    try {
        for (const auto& p : Ort::GetAvailableProviders())
            if (p.find("CUDA") != std::string::npos) { listed = true; break; }
    } catch (...) {}
    if (!listed) return false;
    if (!cudnnSymbolsResolvable()) {
        g_cudaState.store(2, std::memory_order_relaxed);
        if (!g_cudaFallbackReported.exchange(true)) {
            std::fprintf(stderr,
                         "onnx_engine: CUDA provider 依赖的 cuDNN 符号不可用，使用 CPU"
                         "（onnxruntime GPU 包与 cuDNN 版本不匹配？）\n");
        }
        return false;
    }
    return true;
}

} // namespace onnx_engine
