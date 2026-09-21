#pragma once
#include <opencv2/imgproc.hpp>
#include <tensorvia/core/tensor.h>
#include <QString>
#include <QVector>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>
#include "engine/NodeData.hpp"
#include "onnx_engine/types.hpp"

namespace onnx_convert {

// 应用侧中性张量：仅用于**后处理读取**（SDK 已改用 Tensorvia::Tensor，不再有该类型）。
struct TensorBuffer {
    onnx_engine::ElementType type = onnx_engine::ElementType::Unknown;
    std::vector<int64_t> shape;
    std::vector<std::uint8_t> data;
};

// ============================ 失败约定 (failure convention) ============================
// 本头的转换函数在失败时**不抛异常**，而是返回可判定的「空」值，由调用方
// （PreProcess/OnnxInfer 执行器）检测后向用户产出具体错误信息：
//   - imageToTensor()  失败（空图、不支持的 dtype、通道/尺寸与目标形状不符、
//                      layout 无法判定、Float16/BFloat16 未实现）→ 返回默认构造的
//                      空 Tensor（numel()==0）；
//   - tensorToNodeData() 对空张量 → 返回 std::monostate；标量（numel==1）转 bool/double。
// 调用方应把「空 Tensor / monostate」一律视为错误，并在可能的情况下先做校验。
// =====================================================================================

// 设计 §4.1：仅下列类型可被表示（Bool 经 INT8 承载）；
// UInt8/UInt16/UInt32/UInt64/String/Complex/Unknown 不受支持，调用方须先检查。
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
    case onnx_engine::ElementType::Bool:
        return true;
    default:
        return false;
    }
}

namespace detail {

// IEEE 754 half (float16) 位模式 → double。
inline double halfBitsToDouble(std::uint16_t h) {
    const std::uint32_t sign = (h >> 15) & 1u;
    const std::uint32_t exp  = (h >> 10) & 0x1Fu;
    const std::uint32_t mant = h & 0x3FFu;
    double v = 0.0;
    if (exp == 0) {
        if (mant != 0) v = std::ldexp(static_cast<double>(mant), -24);  // 次正规数
    } else if (exp == 0x1F) {
        v = (mant != 0) ? std::numeric_limits<double>::quiet_NaN()
                        : std::numeric_limits<double>::infinity();
    } else {
        v = std::ldexp(static_cast<double>(mant) / 1024.0 + 1.0, static_cast<int>(exp) - 15);
    }
    return sign ? -v : v;
}

// bfloat16 位模式（高 16 位与 float32 相同）→ double。
inline double bfloat16BitsToDouble(std::uint16_t h) {
    const std::uint32_t bits = static_cast<std::uint32_t>(h) << 16;
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(float));
    return static_cast<double>(f);
}

// double → IEEE 754 half / bfloat16 位模式（写张量用）
inline std::uint16_t doubleToHalfBits(double value) {
    float f = static_cast<float>(value);
    std::uint32_t x;
    std::memcpy(&x, &f, 4);
    const std::uint32_t sign = (x >> 16) & 0x8000u;
    int exp = int((x >> 23) & 0xFF) - 127 + 15;
    const std::uint32_t man = x & 0x7FFFFFu;
    if (exp <= 0) {
        if (exp < -10) return static_cast<std::uint16_t>(sign);
        std::uint32_t m = (man | 0x800000u) >> (1 - exp);
        return static_cast<std::uint16_t>(sign | (m >> 13));
    }
    if (exp >= 31) return static_cast<std::uint16_t>(sign | 0x7C00u);
    return static_cast<std::uint16_t>(sign | (static_cast<std::uint32_t>(exp) << 10) | (man >> 13));
}

inline std::uint16_t doubleToBfloat16Bits(double value) {
    float f = static_cast<float>(value);
    std::uint32_t x;
    std::memcpy(&x, &f, 4);
    return static_cast<std::uint16_t>(x >> 16);
}

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
    case onnx_engine::ElementType::Float16: {
        const std::uint16_t h = detail::doubleToHalfBits(v);
        std::memcpy(dst, &h, 2);
        break;
    }
    case onnx_engine::ElementType::BFloat16: {
        const std::uint16_t b = detail::doubleToBfloat16Bits(v);
        std::memcpy(dst, &b, 2);
        break;
    }
    }
}

inline float readScalarAsFloat(const std::uint8_t* src, onnx_engine::ElementType t) {
    switch (t) {
    case onnx_engine::ElementType::Float32:  { float v;    std::memcpy(&v, src, 4); return v; }
    case onnx_engine::ElementType::Float64:  { double v;   std::memcpy(&v, src, 8); return static_cast<float>(v); }
    case onnx_engine::ElementType::Float16:  { std::uint16_t v; std::memcpy(&v, src, 2); return static_cast<float>(halfBitsToDouble(v)); }
    case onnx_engine::ElementType::BFloat16: { std::uint16_t v; std::memcpy(&v, src, 2); return static_cast<float>(bfloat16BitsToDouble(v)); }
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
    case onnx_engine::ElementType::Float32:  { float v;    std::memcpy(&v, src, 4); return static_cast<double>(v); }
    case onnx_engine::ElementType::Float64:  { double v;   std::memcpy(&v, src, 8); return v; }
    case onnx_engine::ElementType::Float16:  { std::uint16_t v; std::memcpy(&v, src, 2); return halfBitsToDouble(v); }
    case onnx_engine::ElementType::BFloat16: { std::uint16_t v; std::memcpy(&v, src, 2); return bfloat16BitsToDouble(v); }
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

} // namespace detail

// cv::Mat(BGR/灰度, 8U) → 模型输入张量（Tensorvia）。
// 布局：targetShape 为 4 维且 shape[1]==3 视为 NCHW，否则 NHWC。
inline Tensor imageToTensor(const cv::Mat& img,
                            const std::vector<int64_t>& targetShape,
                            onnx_engine::ElementType dtype,
                            const QString& norm, const QVector<double>& mean,
                            const QVector<double>& std,
                            const QString& channel, const QString& resize) {
    if (img.empty()) return Tensor{};
    // 不支持的 dtype（UInt16/UInt32/... 或 Unknown）不尝试转换：返回空张量，
    // 由调用方向用户报告「不支持的张量类型」。
    if (!isSupported(dtype)) return Tensor{};

    cv::Mat work = img;

    // 通道序：显式 rgb，或 auto 且三通道时做 BGR→RGB。
    const bool wantRgb = (channel == QStringLiteral("rgb")) ||
                         (channel != QStringLiteral("bgr") && img.channels() == 3);
    if (wantRgb && work.channels() == 3)
        cv::cvtColor(work, work, cv::COLOR_BGR2RGB);

    const std::size_t rank = targetShape.size();
    const int64_t imgChannels = img.channels();
    bool nchw = false;
    if (rank == 4) {
        // 用图像实际通道数判定布局：NCHW 要求 shape[1]==C，NHWC 要求 shape[3]==C。
        const bool asNchw = (targetShape[1] == imgChannels);
        const bool asNhwc = (targetShape[3] == imgChannels);
        if (asNchw == asNhwc) return Tensor{};  // 两者皆真或皆假：无法判定 → 失败
        nchw = asNchw;
    }
    auto get = [&](std::size_t i, int64_t fb) -> int64_t {
        return (i < rank && targetShape[i] > 0) ? targetShape[i] : fb;
    };

    int64_t N = 1, C = 0, H = 0, W = 0;
    if (nchw) {
        N = get(0, 1); H = get(2, img.rows); W = get(3, img.cols);
    } else if (rank == 4) {
        N = get(0, 1); H = get(1, img.rows); W = get(2, img.cols);
    } else {
        H = img.rows; W = img.cols;
    }
    C = imgChannels;
    if (H <= 0) H = img.rows;
    if (W <= 0) W = img.cols;
    if (N <= 0) N = 1;
    if (H <= 0 || W <= 0) return Tensor{};

    if (resize == QStringLiteral("auto")) {
        if (work.rows != H || work.cols != W)
            cv::resize(work, work, cv::Size(static_cast<int>(W), static_cast<int>(H)), 0, 0, cv::INTER_LINEAR);
    } else if (work.rows != H || work.cols != W) {
        // resize != "auto"（如 "keep"）且目标空间维与图像不符：不做缩放，
        // 直接返回空张量，避免后续按 H/W 越界读取 f。
        return Tensor{};
    }

    cv::Mat f;
    work.convertTo(f, CV_32FC(work.channels()));

    const int srcC = f.channels();
    const std::size_t total = static_cast<std::size_t>(N) * static_cast<std::size_t>(C) *
                              static_cast<std::size_t>(H) * static_cast<std::size_t>(W);
    std::vector<float> fbuf(total, 0.0f);

    // 逐通道 Min-Max：先统计各通道最小/最大值
    std::vector<float> cmin(static_cast<std::size_t>(C), 0.0f);
    std::vector<float> cmax(static_cast<std::size_t>(C), 1.0f);
    if (norm == QStringLiteral("minmax")) {
        for (int64_t c = 0; c < C; ++c) {
            double mn = 1e300, mx = -1e300;
            for (int64_t y = 0; y < H; ++y) {
                const float* row = f.ptr<float>(static_cast<int>(y));
                for (int64_t x = 0; x < W; ++x) {
                    const int sc = (srcC == C) ? static_cast<int>(c) : 0;
                    const float v = row[x * srcC + sc];
                    mn = std::min(mn, double(v));
                    mx = std::max(mx, double(v));
                }
            }
            if (mn > mx) { mn = 0.0; mx = 1.0; }
            cmin[static_cast<std::size_t>(c)] = static_cast<float>(mn);
            cmax[static_cast<std::size_t>(c)] = static_cast<float>(mx);
        }
    }

    for (int64_t y = 0; y < H; ++y) {
        const float* row = f.ptr<float>(static_cast<int>(y));
        for (int64_t x = 0; x < W; ++x) {
            for (int64_t c = 0; c < C; ++c) {
                const int sc = (srcC == C) ? static_cast<int>(c) : 0;
                float v = row[x * srcC + sc];
                if (norm == QStringLiteral("div255") || norm == QStringLiteral("unit")) {
                    v /= 255.0f;
                } else if (norm == QStringLiteral("meanstd") || norm == QStringLiteral("zscore")) {
                    // Z-score：(x - mean) / std（按通道）
                    const double m = (c < mean.size()) ? mean[static_cast<int>(c)] : 0.0;
                    const double s = (c < std.size() && std[static_cast<int>(c)] != 0.0)
                                         ? std[static_cast<int>(c)] : 1.0;
                    v = static_cast<float>((static_cast<double>(v) - m) / s);
                } else if (norm == QStringLiteral("pm1")) {
                    // 缩放到 [-1, 1]：x/127.5 - 1
                    v = v / 127.5f - 1.0f;
                } else if (norm == QStringLiteral("minmax")) {
                    const std::size_t ci = static_cast<std::size_t>(c);
                    const float lo = cmin[ci], hi = cmax[ci];
                    const float range = (hi - lo);
                    v = range > 0.0f ? (v - lo) / range : 0.0f;
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

    const via::DataType vdt = onnx_engine::toViaDataType(dtype);
    std::vector<int64_t> outShape;
    if (nchw) outShape = {N, C, H, W};
    else if (rank == 4) outShape = {N, H, W, C};
    else outShape = {N, C, H, W};

    // Tensorvia 不接受空 shape；调用方（预处理任务）保证至少 4 维或图像非空。
    Tensor out(outShape, vdt, via::Device::CPU);
    const std::size_t dstSize = via::calc_dtype_size(vdt);
    const onnx_engine::ElementType dstType = onnx_engine::fromViaDataType(vdt);
    std::uint8_t* dst = static_cast<std::uint8_t*>(out.data());
    for (std::size_t i = 0; i < total; ++i)
        detail::writeScalarAsFloat(dst + i * dstSize, dstType, fbuf[i]);

    return out;
}

// Tensorvia::Tensor → 应用侧中性张量（拷贝到 host 字节缓冲，供后处理读取）。
inline TensorBuffer tensorToBuffer(const Tensor& t) {
    TensorBuffer b;
    b.type = onnx_engine::fromViaDataType(t.dtype());
    const auto span = t.shape();
    b.shape.assign(span.begin(), span.end());

    const std::size_t n = t.numel();
    const std::size_t esize = via::calc_dtype_size(t.dtype());
    if (n > 0 && esize > 0) {
        // 先取连续副本，再确保数据位于 host，避免直接解引用非连续/设备指针。
        // Tensorvia 提供 contiguous() 与 to_host()（均非 const）。本应用为 CPU 后端，
        // to_host() 为无操作；contiguous() 对已连续张量可能返回共享 impl 的浅拷贝。
        Tensor host = t.contiguous();
        host.to_host();
        b.data.resize(n * esize);
        std::memcpy(b.data.data(), host.data(), n * esize);
    }
    return b;
}

// Tensorvia::Tensor → NodeData。
inline NodeData tensorToNodeData(const Tensor& t, onnx_engine::ElementType declType) {
    if (t.numel() == 0) return NodeData{std::monostate{}};

    if (t.numel() == 1) {
        Tensor host = t.contiguous();
        host.to_host();
        if (declType == onnx_engine::ElementType::Bool) {
            std::int8_t v = 0;
            std::memcpy(&v, host.data(), 1);
            return NodeData(v != 0);
        }
        // 按张量自身 dtype 读取（Int8/UInt8/UInt16 等承载宽度可能已被 §4.1 调整）。
        const onnx_engine::ElementType et = onnx_engine::fromViaDataType(t.dtype());
        if (onnx_engine::elementTypeSize(et) <= 0) return NodeData{std::monostate{}};
        return NodeData(detail::readScalarAsDouble(
            static_cast<const std::uint8_t*>(host.data()), et));
    }
    return NodeData(t);
}

// 无声明类型时的退化重载：标量一律 → double（INT8 也按 Int8 处理，不退化为 bool）。
inline NodeData tensorToNodeData(const Tensor& t) {
    return tensorToNodeData(t, onnx_engine::ElementType::Unknown);
}

} // namespace onnx_convert
