#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <tensorvia/core/tensor.h>
#include "onnx_engine/export.hpp"

namespace onnx_engine {

// 张量载体统一用 Tensorvia；推理输入在 CPU 且连续时零拷贝直接喂给 ORT。
using Tensor = ::Tensor;

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

struct SessionOptions {
    Device device = Device::Auto;   // Auto：有 CUDA 则用 CUDA，否则 CPU
    int intraThreads = 0;           // 0 = onnxruntime 默认
};

ONNX_ENGINE_API const char* elementTypeName(ElementType t);

// 返回元素字节大小；Unknown 返回 0
ONNX_ENGINE_API int elementTypeSize(ElementType t);

// ONNX ElementType ↔ Tensorvia DataType（设计 §4.1）。
// Tensorvia 无 UInt8/Bool：UInt8→INT16、Bool→INT8；其余同名。
// 不受支持的类型：toViaDataType 返回 FLOAT32（调用方须自行先判定支持性），
// fromViaDataType 返回 ElementType::Unknown。
ONNX_ENGINE_API via::DataType toViaDataType(ElementType t);
ONNX_ENGINE_API ElementType fromViaDataType(via::DataType t);

} // namespace onnx_engine
