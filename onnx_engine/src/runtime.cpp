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
    return s;
}

std::shared_ptr<SessionImpl> Runtime::Impl::get(const std::string& path,
                                                const SessionOptions& o, std::string& error) {
    error.clear();
    std::error_code ec;
    const std::string abs = normPath(path);
    const auto mt = std::filesystem::last_write_time(abs, ec);
    const auto sz = std::filesystem::file_size(abs, ec);
    if (ec) { error = "模型文件不存在或不可读: " + path; return nullptr; }
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
        building.erase(key);
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
        } else {
            slot->promise.set_exception(std::make_exception_ptr(std::runtime_error(buildError)));
        }
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

bool SessionImpl::run(const std::vector<TensorBuffer>&, std::vector<TensorBuffer>&,
                      std::string& error) {
    error = "onnx_engine: not implemented";
    return false;
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
