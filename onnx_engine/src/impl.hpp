#pragma once
#include <future>
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
    // 会话创建后只读：输入/输出名及其稳定的 c_str 指针，供 run() 直接使用
    std::vector<std::string> inputNames, outputNames;
    std::vector<const char*> inputNamePtrs, outputNamePtrs;

    explicit SessionImpl(std::shared_ptr<Ort::Env> env);
    const ModelInfo& info() const override { return info_; }
    bool run(const std::vector<Tensor>& inputs,
             std::vector<Tensor>& outputs, std::string& error) override;
};

// Runtime 带 ONNX_ENGINE_API(default visibility)，嵌套的 Impl 会继承该可见性；
// 隐藏性由 runtime.hpp 中对 Impl 的前向声明附加 hidden 属性保证，此处为普通定义。
struct Runtime::Impl {
    std::shared_ptr<Ort::Env> env;
    // 锁契约：所有对 sessions/lru/building/maxSessions/maxBytes 的访问都必须持有本锁；
    // 统一在最内层 get() 加锁，外层（Runtime::session 等）不得再加锁，避免死锁（非递归锁）。
    // 注意：会话的实际构造在锁外进行，通过 building+future 协调并发加载。
    std::mutex mutex;
    int maxSessions = 4;
    std::size_t maxBytes = std::size_t(1) << 30;  // 1 GiB
    std::unordered_map<std::string, std::shared_ptr<SessionImpl>> sessions;
    std::list<std::string> lru;   // front = 最近使用
    // 正在构造中的会话（键与 sessions 相同）。leader 在锁外加载，等待者通过 future 取得结果，
    // 从而避免持锁进行图优化/权重加载。用 shared_future 以支持多个并发等待者。
    struct BuildSlot {
        std::promise<std::shared_ptr<SessionImpl>> promise;
        std::shared_future<std::shared_ptr<SessionImpl>> future;
    };
    std::unordered_map<std::string, std::shared_ptr<BuildSlot>> building;

    std::string makeKey(const std::string& path, const SessionOptions& o) const;
    void touch(const std::string& key);
    void evictIfNeeded();
    std::shared_ptr<SessionImpl> get(const std::string& path, const SessionOptions& o,
                                     std::string& error);
    void reloadPath(const std::string& path);
};

} // namespace onnx_engine
