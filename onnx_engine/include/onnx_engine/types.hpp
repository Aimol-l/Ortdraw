#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "onnx_engine/export.hpp"

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
    std::vector<int64_t> shape;   // -1 表示动态维（原样保留）
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

ONNX_ENGINE_API const char* elementTypeName(ElementType t);

// 返回元素字节大小；Unknown 返回 0
ONNX_ENGINE_API int elementTypeSize(ElementType t);

} // namespace onnx_engine
