#include "impl.hpp"
#include <algorithm>
#include <filesystem>
#include <iterator>
#include <stdexcept>

namespace onnx_engine {

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

std::string Runtime::Impl::makeKey(const std::string& path, const SessionOptions& o) const {
    return normPath(path) + "|" + std::to_string(int(o.device)) + "|" + std::to_string(o.intraThreads);
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
    std::lock_guard<std::mutex> lk(mutex);
    error = "onnx_engine: not implemented";
    return nullptr;
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

SessionImpl::SessionImpl(std::shared_ptr<Ort::Env> env) {
    (void)env;
}

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
