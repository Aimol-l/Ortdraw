#include "impl.hpp"
#include <algorithm>
#include <cstdio>
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
    std::string s = "[";
    for (std::size_t i = 0; i < shape.size(); ++i) {
        if (i) s += ",";
        s += std::to_string(shape[i]);
    }
    return s + "]";
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

// path 需为已规范化的绝对路径（调用方保证），避免重复 normPath
std::string Runtime::Impl::makeKey(const std::string& path, const SessionOptions& o) const {
    // 归一化线程数：<0 与 0 语义相同（onnxruntime 默认），避免重复建会话
    const int threads = o.intraThreads > 0 ? o.intraThreads : 0;
    return path + "|" + std::to_string(int(o.device)) + "|" + std::to_string(threads);
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

static std::shared_ptr<SessionImpl> makeSession(std::shared_ptr<Ort::Env> env,
                                                const std::string& absPath,
                                                const SessionOptions& opts,
                                                std::int64_t mtime, std::uint64_t size,
                                                bool useCuda) {
    Ort::SessionOptions so;
    so.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
    if (opts.intraThreads > 0) so.SetIntraOpNumThreads(opts.intraThreads);
    if (useCuda) {
        OrtCUDAProviderOptions c{};
        so.AppendExecutionProvider_CUDA(c);   // 失败则抛异常，由调用方决定回退策略
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
    if (ec) { error = "onnx_engine: 模型文件不存在或不可读: " + path; return nullptr; }
    const auto mtime = std::int64_t(mt.time_since_epoch().count());
    const auto size = std::uint64_t(sz);
    const auto key = makeKey(abs, o) + "|" + std::to_string(mtime) + "|" + std::to_string(size);

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
                           || (o.device == Device::Auto && Runtime::cudaAvailable());
        try {
            s = makeSession(env, abs, o, mtime, size, wantCuda);
        } catch (const std::exception& e) {
            if (o.device == Device::Auto && wantCuda) {
                std::fprintf(stderr, "onnx_engine: CUDA 会话创建失败，回退 CPU: %s\n", e.what());
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

bool SessionImpl::run(const std::vector<TensorBuffer>& inputs, std::vector<TensorBuffer>& outputs,
                      std::string& error) {
    outputs.clear();   // 失败时也保证输出为空，契约干净
    if (inputs.size() != info_.inputs.size()) {
        error = "onnx_engine: 输入个数不符，期望 " + std::to_string(info_.inputs.size())
              + "，实际 " + std::to_string(inputs.size());
        return false;
    }
    try {
        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> ortInputs;
        ortInputs.reserve(inputs.size());
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const TensorBuffer& b = inputs[i];
            const TensorInfo& want = info_.inputs[i];
            if (!want.isTensor) {
                error = "onnx_engine: 输入 " + want.name + " 不是张量";
                return false;
            }
            if (b.type == ElementType::Unknown || elementTypeSize(b.type) == 0) {
                error = "onnx_engine: 输入 " + want.name + " 的元素类型不受支持";
                return false;
            }
            if (b.type != want.type) {
                error = "onnx_engine: 输入 " + want.name + " 类型不符，期望 "
                      + elementTypeName(want.type) + "，实际 " + elementTypeName(b.type);
                return false;
            }
            // 形状校验：秩必须一致；期望维 -1（动态）接受任意，其余必须相等
            if (b.shape.size() != want.shape.size()) {
                error = "onnx_engine: 输入 " + want.name + " 秩不符，期望 "
                      + std::to_string(want.shape.size()) + "，实际 "
                      + std::to_string(b.shape.size());
                return false;
            }
            for (std::size_t d = 0; d < b.shape.size(); ++d) {
                if (want.shape[d] != -1 && b.shape[d] != want.shape[d]) {
                    error = "onnx_engine: 输入 " + want.name + " 形状不符，期望 "
                          + shapeStr(want.shape) + "，实际 " + shapeStr(b.shape);
                    return false;
                }
            }
            // 预校验 data 字节数 == 形状元素数 * 元素大小
            std::size_t elems = 1;
            for (int64_t dm : b.shape) {
                if (dm < 0) {
                    error = "onnx_engine: 输入 " + want.name + " 形状含非法维 "
                          + shapeStr(b.shape);
                    return false;
                }
                elems *= std::size_t(dm);
            }
            const std::size_t wantBytes = elems * std::size_t(elementTypeSize(b.type));
            if (b.data.size() != wantBytes) {
                error = "onnx_engine: 输入 " + want.name + " 数据大小不符，期望 "
                      + std::to_string(wantBytes) + " 字节，实际 "
                      + std::to_string(b.data.size()) + " 字节";
                return false;
            }
            // buffer 由调用方保证在 Run 期间存活（inputs 为 const 引用，同步调用）
            ortInputs.emplace_back(Ort::Value::CreateTensor(
                mem, const_cast<std::uint8_t*>(b.data.data()), b.data.size(),
                b.shape.data(), b.shape.size(), ortElementType(b.type)));
        }

        std::vector<Ort::Value> res =
            ort.Run(Ort::RunOptions{nullptr}, inputNamePtrs.data(), ortInputs.data(),
                    ortInputs.size(), outputNamePtrs.data(), outputNamePtrs.size());

        if (res.size() != info_.outputs.size()) {
            error = "onnx_engine: 输出个数不符，期望 " + std::to_string(info_.outputs.size())
                  + "，实际 " + std::to_string(res.size());
            return false;
        }
        outputs.clear();
        outputs.reserve(res.size());
        for (std::size_t i = 0; i < res.size(); ++i) {
            const TensorInfo& want = info_.outputs[i];
            Ort::Value& v = res[i];
            if (!want.isTensor || !v.IsTensor()) {
                error = "onnx_engine: 输出 " + want.name + " 不是张量，暂不支持";
                return false;
            }
            Ort::TensorTypeAndShapeInfo ti = v.GetTensorTypeAndShapeInfo();
            TensorBuffer b;
            b.type = mapElementType(ti.GetElementType());
            if (b.type == ElementType::Unknown || elementTypeSize(b.type) == 0) {
                error = "onnx_engine: 输出 " + want.name + " 的元素类型不受支持";
                return false;
            }
            b.shape = ti.GetShape();
            const std::size_t bytes =
                std::size_t(ti.GetElementCount()) * std::size_t(elementTypeSize(b.type));
            const std::uint8_t* p = v.GetTensorData<std::uint8_t>();
            if (bytes != 0) b.data.assign(p, p + bytes);
            outputs.push_back(std::move(b));
        }
        error.clear();
        return true;
    } catch (const std::exception& e) {
        outputs.clear();
        error = e.what();
        return false;
    }
}

Runtime::Runtime() : m_impl(std::make_unique<Impl>()) {
    m_impl->env = std::make_shared<Ort::Env>(ORT_LOGGING_LEVEL_WARNING, "ortdraw");
}
Runtime::~Runtime() = default;

Runtime& Runtime::instance() { static Runtime r; return r; }

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
    try {
        auto providers = Ort::GetAvailableProviders();
        for (const auto& p : providers)
            if (p.find("CUDA") != std::string::npos) return true;
    } catch (...) {}
    return false;
}

} // namespace onnx_engine
