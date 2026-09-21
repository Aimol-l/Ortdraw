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
    std::shared_ptr<Ort::Env> env;
    Ort::Session ort{nullptr};
    ModelInfo info_;
    std::string path;
    std::int64_t mtime = 0;
    std::uint64_t size = 0;
    std::size_t weightBytes = 0;   // 估算权重（= 模型文件大小）

    explicit SessionImpl(std::shared_ptr<Ort::Env> env);
    const ModelInfo& info() const override { return info_; }
    bool run(const std::vector<TensorBuffer>& inputs,
             std::vector<TensorBuffer>& outputs, std::string& error) override;
};

struct Runtime::Impl {
    std::shared_ptr<Ort::Env> env;
    // 锁契约：所有对 sessions/lru/maxSessions/maxBytes 的访问都必须持有本锁；
    // 统一在最内层 get() 加锁，外层（Runtime::session 等）不得再加锁，避免死锁（非递归锁）。
    std::mutex mutex;
    int maxSessions = 4;
    std::size_t maxBytes = std::size_t(1) << 30;  // 1 GiB
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
