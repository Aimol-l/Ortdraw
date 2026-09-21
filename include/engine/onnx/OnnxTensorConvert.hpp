#pragma once
#include <opencv2/imgproc.hpp>
#include <tensorvia/core/tensor.h>
#include <QString>
#include <QVector>
#include <cstring>
#include <vector>
#include "engine/NodeData.hpp"
#include "onnx_engine/types.hpp"

namespace onnx_convert {

// ONNX ElementType ↔ Tensorvia DataType（设计 §4.1）。
// Tensorvia 没有 UInt8/Bool：UInt8→INT16，Bool→INT8；其余不支持类型→FLOAT32。
inline via::DataType toViaType(onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Float32:  return via::DataType::FLOAT32;
    case onnx_engine::ElementType::Float64:  return via::DataType::FLOAT64;
    case onnx_engine::ElementType::Float16:  return via::DataType::FLOAT16;
    case onnx_engine::ElementType::BFloat16: return via::DataType::BFLOAT16;
    case onnx_engine::ElementType::Int8:     return via::DataType::INT8;
    case onnx_engine::ElementType::Int16:    return via::DataType::INT16;
    case onnx_engine::ElementType::Int32:    return via::DataType::INT32;
    case onnx_engine::ElementType::Int64:    return via::DataType::INT64;
    case onnx_engine::ElementType::UInt8:    return via::DataType::INT16;
    case onnx_engine::ElementType::Bool:     return via::DataType::INT8;
    default:                                 return via::DataType::FLOAT32;
    }
}

inline onnx_engine::ElementType fromViaType(via::DataType t) {
    switch (t) {
    case via::DataType::FLOAT32:  return onnx_engine::ElementType::Float32;
    case via::DataType::FLOAT64:  return onnx_engine::ElementType::Float64;
    case via::DataType::FLOAT16:  return onnx_engine::ElementType::Float16;
    case via::DataType::BFLOAT16: return onnx_engine::ElementType::BFloat16;
    case via::DataType::INT8:     return onnx_engine::ElementType::Int8;
    case via::DataType::INT16:    return onnx_engine::ElementType::Int16;
    case via::DataType::INT32:    return onnx_engine::ElementType::Int32;
    case via::DataType::INT64:    return onnx_engine::ElementType::Int64;
    default:                      return onnx_engine::ElementType::Unknown;
    }
}

// 设计 §4.1：仅下列类型可被表示（UInt8/Bool 经映射承载）；
// UInt16/UInt32/UInt64/String/Complex/Unknown 不受支持，调用方须先检查。
inline bool isSupported(onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Float32:
    case onnx_engine::ElementType::Float64:
    case onnx_engine::ElementType::Float16:
    case onnx_engine::ElementType::BFloat16:
    case onnx_engine::ElementType::Int8:
    case onnx_engine::ElementType::Int16:
    case onnx_engine::ElementType::Int32:
    case onnx_engine::ElementType::Int64:
    case onnx_engine::ElementType::UInt8:
    case onnx_engine::ElementType::Bool:
        return true;
    default:
        return false;
    }
}

namespace detail {

// 把一段连续的 float 值按目标 dtype 写进行主序字节缓冲（单元素）。
inline void writeScalarAsFloat(std::uint8_t* dst, onnx_engine::ElementType t, float v) {
    switch (t) {
    case onnx_engine::ElementType::Float32: {
        std::memcpy(dst, &v, sizeof(float));
        break;
    }
    case onnx_engine::ElementType::Float64: {
        const double d = static_cast<double>(v);
        std::memcpy(dst, &d, sizeof(double));
        break;
    }
    case onnx_engine::ElementType::Int8: {
        const std::int8_t x = static_cast<std::int8_t>(v);
        std::memcpy(dst, &x, 1);
        break;
    }
    case onnx_engine::ElementType::Int16: {
        const std::int16_t x = static_cast<std::int16_t>(v);
        std::memcpy(dst, &x, 2);
        break;
    }
    case onnx_engine::ElementType::Int32: {
        const std::int32_t x = static_cast<std::int32_t>(v);
        std::memcpy(dst, &x, 4);
        break;
    }
    case onnx_engine::ElementType::Int64: {
        const std::int64_t x = static_cast<std::int64_t>(v);
        std::memcpy(dst, &x, 8);
        break;
    }
    case onnx_engine::ElementType::UInt8: {
        const std::uint8_t x = static_cast<std::uint8_t>(v < 0 ? 0 : v > 255 ? 255 : v);
        std::memcpy(dst, &x, 1);
        break;
    }
    case onnx_engine::ElementType::Bool: {
        const std::uint8_t x = v != 0.0f ? 1 : 0;
        std::memcpy(dst, &x, 1);
        break;
    }
    default:
        // Float16/BFloat16 等暂不在此处转换，调用方应返回空缓冲。
        break;
    }
}

inline float readScalarAsFloat(const std::uint8_t* src, onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Float32:  { float v;    std::memcpy(&v, src, 4); return v; }
    case onnx_engine::ElementType::Float64:  { double v;   std::memcpy(&v, src, 8); return static_cast<float>(v); }
    case onnx_engine::ElementType::Float16:  { std::uint16_t v; std::memcpy(&v, src, 2); return static_cast<float>(v); }
    case onnx_engine::ElementType::BFloat16: { std::uint16_t v; std::memcpy(&v, src, 2); return static_cast<float>(v); }
    case onnx_engine::ElementType::Int8:     { std::int8_t v;   std::memcpy(&v, src, 1); return static_cast<float>(v); }
    case onnx_engine::ElementType::Int16:    { std::int16_t v;  std::memcpy(&v, src, 2); return static_cast<float>(v); }
    case onnx_engine::ElementType::Int32:    { std::int32_t v;  std::memcpy(&v, src, 4); return static_cast<float>(v); }
    case onnx_engine::ElementType::Int64:    { std::int64_t v;  std::memcpy(&v, src, 8); return static_cast<float>(v); }
    case onnx_engine::ElementType::UInt8:    return static_cast<float>(src[0]);
    case onnx_engine::ElementType::Bool:     return src[0] != 0 ? 1.0f : 0.0f;
    default:                                 return 0.0f;
    }
}

inline double readScalarAsDouble(const std::uint8_t* src, onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Int8:   { std::int8_t v;   std::memcpy(&v, src, 1); return static_cast<double>(v); }
    case onnx_engine::ElementType::Int16:  { std::int16_t v;  std::memcpy(&v, src, 2); return static_cast<double>(v); }
    case onnx_engine::ElementType::Int32:  { std::int32_t v;  std::memcpy(&v, src, 4); return static_cast<double>(v); }
    case onnx_engine::ElementType::Int64:  { std::int64_t v;  std::memcpy(&v, src, 8); return static_cast<double>(v); }
    case onnx_engine::ElementType::UInt8:  return static_cast<double>(src[0]);
    case onnx_engine::ElementType::UInt16: { std::uint16_t v; std::memcpy(&v, src, 2); return static_cast<double>(v); }
    case onnx_engine::ElementType::UInt32: { std::uint32_t v; std::memcpy(&v, src, 4); return static_cast<double>(v); }
    case onnx_engine::ElementType::UInt64: { std::uint64_t v; std::memcpy(&v, src, 8); return static_cast<double>(v); }
    case onnx_engine::ElementType::Bool:   return src[0] != 0 ? 1.0 : 0.0;
    default:                               return 0.0;
    }
}

inline bool isIntegerType(onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Int8:
    case onnx_engine::ElementType::Int16:
    case onnx_engine::ElementType::Int32:
    case onnx_engine::ElementType::Int64:
    case onnx_engine::ElementType::UInt8:
    case onnx_engine::ElementType::UInt16:
    case onnx_engine::ElementType::UInt32:
    case onnx_engine::ElementType::UInt64:
        return true;
    default:
        return false;
    }
}

inline std::size_t numelOf(const std::vector<int64_t>& shape) {
    std::size_t n = 1;
    for (int64_t d : shape) n *= static_cast<std::size_t>(d > 0 ? d : 1);
    return n;
}

// 由 TensorBuffer 构造 Tensorvia::Tensor（按 §4.1 映射 dtype；UInt8 按无符号解释转 INT16）。
inline Tensor bufferToTensor(const onnx_engine::TensorBuffer& b) {
    const std::vector<int64_t> shape = b.shape;
    Tensor t(shape, toViaType(b.type));
    const std::size_t n = numelOf(shape);
    if (n == 0 || b.data.empty()) return t;

    const std::size_t srcSize = static_cast<std::size_t>(onnx_engine::elementTypeSize(b.type));
    const std::size_t dstSize = via::calc_dtype_size(t.dtype());
    if (srcSize == 0) return t;

    if (srcSize == dstSize) {
        std::memcpy(t.data(), b.data.data(), n * srcSize);
    } else {
        std::uint8_t* dst = static_cast<std::uint8_t*>(t.data());
        for (std::size_t i = 0; i < n; ++i) {
            const float v = readScalarAsFloat(b.data.data() + i * srcSize, b.type);
            writeScalarAsFloat(dst + i * dstSize, fromViaType(t.dtype()), v);
        }
    }
    return t;
}

} // namespace detail

// cv::Mat(BGR/灰度, 8U) → 模型输入张量。
// 布局：targetShape 为 4 维且 shape[1]==3 视为 NCHW，否则 NHWC。
inline onnx_engine::TensorBuffer imageToTensor(const cv::Mat& img,
                                               const std::vector<int64_t>& targetShape,
                                               onnx_engine::ElementType dtype,
                                               const QString& norm, const QVector<double>& mean,
                                               const QVector<double>& std,
                                               const QString& channel, const QString& resize) {
    onnx_engine::TensorBuffer out;
    if (img.empty()) return out;
    // 不支持的 dtype（UInt16/UInt32/... 或 Unknown）不尝试转换：返回空缓冲，
    // 由调用方向用户报告「不支持的张量类型」。
    if (!isSupported(dtype)) return out;

    cv::Mat work = img;

    // 通道序：显式 rgb，或 auto 且三通道时做 BGR→RGB。
    const bool wantRgb = (channel == QStringLiteral("rgb")) ||
                         (channel != QStringLiteral("bgr") && img.channels() == 3);
    if (wantRgb && work.channels() == 3)
        cv::cvtColor(work, work, cv::COLOR_BGR2RGB);

    const std::size_t rank = targetShape.size();
    const bool nchw = (rank == 4 && targetShape[1] == 3);
    auto get = [&](std::size_t i, int64_t fb) -> int64_t {
        return (i < rank && targetShape[i] > 0) ? targetShape[i] : fb;
    };

    int64_t N = 1, C = 0, H = 0, W = 0;
    if (nchw) {
        N = get(0, 1); C = get(1, img.channels()); H = get(2, img.rows); W = get(3, img.cols);
    } else if (rank == 4) {
        N = get(0, 1); H = get(1, img.rows); W = get(2, img.cols); C = get(3, img.channels());
    } else {
        H = img.rows; W = img.cols; C = img.channels();
    }
    if (C <= 0) C = img.channels();
    if (H <= 0) H = img.rows;
    if (W <= 0) W = img.cols;
    if (N <= 0) N = 1;
    if (H <= 0 || W <= 0) return out;

    if (resize == QStringLiteral("auto")) {
        if (work.rows != H || work.cols != W)
            cv::resize(work, work, cv::Size(static_cast<int>(W), static_cast<int>(H)), 0, 0, cv::INTER_LINEAR);
    } else if (work.rows != H || work.cols != W) {
        // resize != "auto"（如 "keep"）且目标空间维与图像不符：不做缩放，
        // 直接返回空缓冲，避免后续按 H/W 越界读取 f。
        return out;
    }

    cv::Mat f;
    work.convertTo(f, CV_32FC(work.channels()));

    const int srcC = f.channels();
    const std::size_t total = static_cast<std::size_t>(N) * static_cast<std::size_t>(C) *
                              static_cast<std::size_t>(H) * static_cast<std::size_t>(W);
    std::vector<float> fbuf(total, 0.0f);

    for (int64_t y = 0; y < H; ++y) {
        const float* row = f.ptr<float>(static_cast<int>(y));
        for (int64_t x = 0; x < W; ++x) {
            for (int64_t c = 0; c < C; ++c) {
                const int sc = (srcC == C) ? static_cast<int>(c) : 0;
                float v = row[x * srcC + sc];
                if (norm == QStringLiteral("div255")) {
                    v /= 255.0f;
                } else if (norm == QStringLiteral("meanstd")) {
                    const double m = (c < mean.size()) ? mean[static_cast<int>(c)] : 0.0;
                    const double s = (c < std.size() && std[static_cast<int>(c)] != 0.0)
                                         ? std[static_cast<int>(c)] : 1.0;
                    v = static_cast<float>((static_cast<double>(v) - m) / s);
                }
                const std::size_t idx = nchw
                    ? (static_cast<std::size_t>(c) * static_cast<std::size_t>(H) + static_cast<std::size_t>(y)) *
                          static_cast<std::size_t>(W) + static_cast<std::size_t>(x)
                    : (static_cast<std::size_t>(y) * static_cast<std::size_t>(W) + static_cast<std::size_t>(x)) *
                          static_cast<std::size_t>(C) + static_cast<std::size_t>(c);
                fbuf[idx] = v;
            }
        }
    }

    const int esize = onnx_engine::elementTypeSize(dtype);
    if (esize <= 0) return out;

    out.type = dtype;
    out.shape.clear();
    if (nchw) out.shape = {N, C, H, W};
    else if (rank == 4) out.shape = {N, H, W, C};
    else out.shape = {N, C, H, W};

    out.data.resize(total * static_cast<std::size_t>(esize));
    for (std::size_t i = 0; i < total; ++i)
        detail::writeScalarAsFloat(out.data.data() + i * static_cast<std::size_t>(esize), dtype, fbuf[i]);

    // Float16/BFloat16 未实现转换，写出全零无意义，按约定返回空缓冲。
    if (dtype == onnx_engine::ElementType::Float16 || dtype == onnx_engine::ElementType::BFloat16)
        return onnx_engine::TensorBuffer{};

    return out;
}

// Tensorvia::Tensor → 中性张量（拷贝到 host 字节缓冲）。
inline onnx_engine::TensorBuffer tensorToBuffer(const Tensor& t) {
    onnx_engine::TensorBuffer b;
    b.type = fromViaType(t.dtype());
    const auto span = t.shape();
    b.shape.assign(span.begin(), span.end());

    const std::size_t n = t.numel();
    const std::size_t esize = via::calc_dtype_size(t.dtype());
    if (n > 0 && esize > 0) {
        // Tensorvia 提供 to_host()（非 const）。复制一份并确保数据位于 host 后再取指针，
        // 避免在不同后端（CUDA/SYCL/...）下直接解引用设备指针。本应用为 CPU 后端，to_host() 为无操作。
        Tensor host = t;
        host.to_host();
        b.data.resize(n * esize);
        std::memcpy(b.data.data(), host.data(), n * esize);
    }
    return b;
}

// 中性张量 → NodeData：Bool 标量→bool；整型标量→double；否则→Tensor。
// 不支持的 dtype（UInt16/UInt32/...）返回 monostate，调用方须检查并报告错误。
inline NodeData tensorToNodeData(const onnx_engine::TensorBuffer& b) {
    if (!isSupported(b.type)) return NodeData{std::monostate{}};
    if (b.shape.empty()) {
        if (b.type == onnx_engine::ElementType::Bool)
            return b.data.empty() ? NodeData(false) : NodeData(b.data[0] != 0);
        if (detail::isIntegerType(b.type) && !b.data.empty())
            return NodeData(detail::readScalarAsDouble(b.data.data(), b.type));
    }
    return detail::bufferToTensor(b);
}

} // namespace onnx_convert
