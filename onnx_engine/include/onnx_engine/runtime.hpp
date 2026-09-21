#pragma once
#include <memory>
#include <string>
#include "onnx_engine/types.hpp"

namespace onnx_engine {

class ONNX_ENGINE_API Session {
public:
    virtual ~Session() = default;
    virtual const ModelInfo& info() const = 0;
    // inputs 按 info().inputs 顺序；outputs 按 info().outputs 顺序返回
    virtual bool run(const std::vector<TensorBuffer>& inputs,
                     std::vector<TensorBuffer>& outputs,
                     std::string& error) = 0;
};

class ONNX_ENGINE_API Runtime {
public:
    static Runtime& instance();

    // 读取模型 IO 元数据（内部缓存；文件变化自动失效重建）
    ModelInfo modelInfo(const std::string& path, const SessionOptions& opts = {});
    // 同上，但失败时通过 error 报告（成功则清空 error）
    ModelInfo modelInfo(const std::string& path, const SessionOptions& opts,
                        std::string& error);
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
    // Impl 为内部实现，禁止导出其符号（否则会泄漏 future/hashtable 等内部符号）
#if defined(__GNUC__)
    struct __attribute__((visibility("hidden"))) Impl;
#else
    struct Impl;
#endif
    std::unique_ptr<Impl> m_impl;
};

} // namespace onnx_engine
