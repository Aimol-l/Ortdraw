#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>
#include <opencv2/imgproc.hpp>
#include "engine/onnx/OnnxTensorConvert.hpp"
#include "engine/tasks/TaskSpec.hpp"

// 预处理任务注册表（单例，内联存储，风格同 NodeRegistry）。
// registerBuiltinPreProcessTasks() 注册内置任务，多次调用幂等。
class PreProcessRegistry {
public:
    static PreProcessRegistry& instance() { static PreProcessRegistry r; return r; }

    void add(TaskSpec s) {
        if (find(s.id)) return;   // 同 id 重复注册忽略
        m_list.push_back(std::move(s));
    }

    const QVector<TaskSpec>& all() const { return m_list; }

    const TaskSpec* find(const QString& id) const {
        for (const auto& s : m_list)
            if (s.id == id) return &s;
        return nullptr;
    }

private:
    QVector<TaskSpec> m_list;
};

namespace preprocess_detail {

// "0.485,0.456,0.406" → [0.485,0.456,0.406]；空串或非法项忽略
inline QVector<double> parseDoubles(const QString& text) {
    QVector<double> out;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool ok = false;
        const double v = part.trimmed().toDouble(&ok);
        if (ok) out.push_back(v);
    }
    return out;
}

inline const cv::Mat* imageInput(const QVector<NodeData>& inputs, int idx = 0) {
    if (idx < 0 || idx >= inputs.size()) return nullptr;
    if (!std::holds_alternative<cv::Mat>(inputs[idx])) return nullptr;
    return &std::get<cv::Mat>(inputs[idx]);
}

// 逐元素归一化：y = (x - mean) / std * scale（4D 时按通道取 mean/std，否则取首值）
inline onnx_engine::TensorBuffer normalizeBuffer(const onnx_engine::TensorBuffer& in,
                                                 double scale,
                                                 const QVector<double>& mean,
                                                 const QVector<double>& stdev) {
    const int esize = onnx_engine::elementTypeSize(in.type);
    const std::size_t n = onnx_convert::detail::numelOf(in.shape);
    if (esize <= 0 || in.data.size() != n * std::size_t(esize)) return {};

    onnx_engine::TensorBuffer out = in;
    bool nchw = false;
    int64_t C = 1, inner = 1;
    if (in.shape.size() == 4) {
        nchw = (in.shape[1] == 3);
        C = nchw ? in.shape[1] : in.shape[3];
        inner = nchw ? (in.shape[2] * in.shape[3]) : 1;
    }
    for (std::size_t i = 0; i < n; ++i) {
        float v = onnx_convert::detail::readScalarAsFloat(in.data.data() + i * esize, in.type);
        double m = mean.isEmpty() ? 0.0 : mean[0];
        double s = stdev.isEmpty() ? 1.0 : stdev[0];
        if (in.shape.size() == 4 && C > 0) {
            const int64_t c = nchw ? int64_t((i / inner) % C) : int64_t(i % C);
            if (int(c) < mean.size()) m = mean[int(c)];
            if (int(c) < stdev.size()) s = stdev[int(c)];
        }
        if (s == 0.0) s = 1.0;
        v = float((double(v) - m) / s * scale);
        onnx_convert::detail::writeScalarAsFloat(out.data.data() + i * esize, in.type, v);
    }
    return out;
}

// 4D 张量空间维缩放（NCHW/NHWC 自动判定），逐通道用 cv::resize（INTER_LINEAR）
inline onnx_engine::TensorBuffer resizeBuffer(const onnx_engine::TensorBuffer& in,
                                              int newW, int newH) {
    if (in.shape.size() != 4 || newW <= 0 || newH <= 0) return {};
    const int esize = onnx_engine::elementTypeSize(in.type);
    const std::size_t n = onnx_convert::detail::numelOf(in.shape);
    if (esize <= 0 || in.data.size() != n * std::size_t(esize)) return {};

    const bool nchw = (in.shape[1] == 3);
    const int64_t N = in.shape[0];
    const int64_t C = nchw ? in.shape[1] : in.shape[3];
    const int64_t H = nchw ? in.shape[2] : in.shape[1];
    const int64_t W = nchw ? in.shape[3] : in.shape[2];
    if (N <= 0 || C <= 0 || H <= 0 || W <= 0) return {};

    onnx_engine::TensorBuffer out;
    out.type = in.type;
    out.shape = nchw ? std::vector<int64_t>{N, C, newH, newW}
                     : std::vector<int64_t>{N, newH, newW, C};
    out.data.resize(std::size_t(N) * C * newH * newW * std::size_t(esize));

    for (int64_t nb = 0; nb < N; ++nb) {
        for (int64_t c = 0; c < C; ++c) {
            cv::Mat plane(int(H), int(W), CV_32F);
            for (int64_t y = 0; y < H; ++y)
                for (int64_t x = 0; x < W; ++x) {
                    const std::size_t si = nchw
                        ? std::size_t(((nb * C + c) * H + y) * W + x)
                        : std::size_t(((nb * H + y) * W + x) * C + c);
                    plane.at<float>(int(y), int(x)) =
                        onnx_convert::detail::readScalarAsFloat(in.data.data() + si * esize, in.type);
                }
            cv::Mat rs;
            cv::resize(plane, rs, cv::Size(newW, newH), 0, 0, cv::INTER_LINEAR);
            for (int64_t y = 0; y < newH; ++y)
                for (int64_t x = 0; x < newW; ++x) {
                    const std::size_t di = nchw
                        ? std::size_t(((nb * C + c) * newH + y) * newW + x)
                        : std::size_t(((nb * newH + y) * newW + x) * C + c);
                    onnx_convert::detail::writeScalarAsFloat(
                        out.data.data() + di * esize, out.type, rs.at<float>(int(y), int(x)));
                }
        }
    }
    return out;
}

// 中性张量 → NodeData 输出；失败时返回错误
inline bool pushTensor(const onnx_engine::TensorBuffer& buf, ExecResult& r, const QString& err) {
    const NodeData d = onnx_convert::tensorToNodeData(buf);
    if (std::holds_alternative<std::monostate>(d)) {
        r.ok = false;
        r.error = err;
        return false;
    }
    r.outputs.push_back(d);
    return true;
}

} // namespace preprocess_detail

inline void registerBuiltinPreProcessTasks() {
    auto& reg = PreProcessRegistry::instance();
    if (reg.find(QStringLiteral("image_to_tensor"))) return;

    using namespace preprocess_detail;

    // ---- 图像 → 张量 ----
    {
        TaskSpec s;
        s.id = "image_to_tensor";
        s.name = QStringLiteral("图像→张量");
        s.inputs = {{QStringLiteral("图像"), DataType::Image}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{
            {"layout", "NCHW"}, {"channel", "auto"}, {"norm", "none"},
            {"mean", "0.485,0.456,0.406"}, {"std", "0.229,0.224,0.225"},
            {"resize", "auto"}, {"width", 224}, {"height", 224},
        };
        s.params = {
            {"layout", QStringLiteral("布局"), "select", "NCHW",
             {{"NCHW", "NCHW"}, {"NHWC", "NHWC"}}},
            {"channel", QStringLiteral("通道"), "select", "auto",
             {{"auto", "auto"}, {"rgb", "rgb"}, {"bgr", "bgr"}}},
            {"norm", QStringLiteral("归一化"), "select", "none",
             {{"none", "none"}, {"div255", "div255"}, {"meanstd", "meanstd"}}},
            {"mean", QStringLiteral("均值"), "text", "0.485,0.456,0.406", {}},
            {"std", QStringLiteral("标准差"), "text", "0.229,0.224,0.225", {}},
            {"resize", QStringLiteral("缩放"), "select", "auto",
             {{"auto", "auto"}, {"keep", "keep"}}},
            {"width", QStringLiteral("宽"), "int", 224, {}},
            {"height", QStringLiteral("高"), "int", 224, {}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            const cv::Mat* img = preprocess_detail::imageInput(inputs, 0);
            if (!img) return {false, QStringLiteral("输入不是图像"), {}};
            if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

            const QString layout = p.value("layout", "NCHW").toString();
            const QString channel = p.value("channel", "auto").toString();
            const QString norm = p.value("norm", "none").toString();
            const QString resize = p.value("resize", "auto").toString();
            const int W = p.value("width", 224).toInt();
            const int H = p.value("height", 224).toInt();
            const QVector<double> mean = parseDoubles(p.value("mean").toString());
            const QVector<double> stdev = parseDoubles(p.value("std").toString());

            const int channels = (channel == QStringLiteral("rgb") || channel == QStringLiteral("bgr"))
                                     ? 3 : img->channels();
            std::vector<int64_t> shape;
            if (layout == QStringLiteral("NHWC"))
                shape = {1, H, W, channels};
            else
                shape = {1, channels, H, W};

            const auto buf = onnx_convert::imageToTensor(
                *img, shape, onnx_engine::ElementType::Float32,
                norm, mean, stdev, channel, resize);
            if (buf.data.empty())
                return {false, QStringLiteral("图像→张量失败（通道或尺寸不匹配）"), {}};

            ExecResult r;
            pushTensor(buf, r, QStringLiteral("图像→张量失败（数据无效）"));
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- YOLO letterbox ----
    {
        TaskSpec s;
        s.id = "yolo_letterbox";
        s.name = QStringLiteral("YOLO Letterbox");
        s.inputs = {{QStringLiteral("图像"), DataType::Image}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{{"size", 640}, {"pad", 114}, {"channel", "rgb"}};
        s.params = {
            {"size", QStringLiteral("尺寸"), "int", 640, {}},
            {"pad", QStringLiteral("填充值"), "int", 114, {}},
            {"channel", QStringLiteral("通道"), "select", "rgb",
             {{"rgb", "rgb"}, {"bgr", "bgr"}}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            const cv::Mat* img = preprocess_detail::imageInput(inputs, 0);
            if (!img) return {false, QStringLiteral("输入不是图像"), {}};
            if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

            const int size = p.value("size", 640).toInt();
            const int pad = p.value("pad", 114).toInt();
            const QString channel = p.value("channel", "rgb").toString();
            if (size <= 0) return {false, QStringLiteral("letterbox 尺寸无效"), {}};

            cv::Mat bgr;
            if (img->channels() == 1)
                cv::cvtColor(*img, bgr, cv::COLOR_GRAY2BGR);
            else if (img->channels() == 4)
                cv::cvtColor(*img, bgr, cv::COLOR_BGRA2BGR);
            else
                bgr = *img;
            if (channel == QStringLiteral("rgb"))
                cv::cvtColor(bgr, bgr, cv::COLOR_BGR2RGB);

            const double scale = std::min(double(size) / bgr.cols, double(size) / bgr.rows);
            const int nw = std::max(1, int(std::lround(bgr.cols * scale)));
            const int nh = std::max(1, int(std::lround(bgr.rows * scale)));
            cv::Mat resized;
            cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);

            cv::Mat f;
            resized.convertTo(f, CV_32FC3, 1.0 / 255.0);
            const int top = (size - nh) / 2, bottom = size - nh - top;
            const int left = (size - nw) / 2, right = size - nw - left;
            cv::Mat canvas;
            const double pv = pad / 255.0;
            cv::copyMakeBorder(f, canvas, top, bottom, left, right,
                               cv::BORDER_CONSTANT, cv::Scalar(pv, pv, pv));

            onnx_engine::TensorBuffer buf;
            buf.type = onnx_engine::ElementType::Float32;
            buf.shape = {1, 3, size, size};
            buf.data.resize(std::size_t(3) * size * size * sizeof(float));
            float* dst = reinterpret_cast<float*>(buf.data.data());
            for (int y = 0; y < size; ++y) {
                const float* row = canvas.ptr<float>(y);
                for (int x = 0; x < size; ++x)
                    for (int c = 0; c < 3; ++c)
                        dst[(std::size_t(c) * size + y) * size + x] = row[x * 3 + c];
            }

            ExecResult r;
            pushTensor(buf, r, QStringLiteral("letterbox 失败"));
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- 归一化 ----
    {
        TaskSpec s;
        s.id = "normalize";
        s.name = QStringLiteral("归一化");
        s.inputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{{"scale", 1.0}, {"mean", "0,0,0"}, {"std", "1,1,1"}};
        s.params = {
            {"scale", QStringLiteral("缩放"), "float", 1.0, {}},
            {"mean", QStringLiteral("均值"), "text", "0,0,0", {}},
            {"std", QStringLiteral("标准差"), "text", "1,1,1", {}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            if (inputs.isEmpty() || !std::holds_alternative<Tensor>(inputs[0]))
                return {false, QStringLiteral("输入不是张量"), {}};
            const auto in = onnx_convert::tensorToBuffer(std::get<Tensor>(inputs[0]));
            const auto out = normalizeBuffer(in, p.value("scale", 1.0).toDouble(),
                                             parseDoubles(p.value("mean").toString()),
                                             parseDoubles(p.value("std").toString()));
            if (out.data.empty()) return {false, QStringLiteral("归一化失败（张量数据无效）"), {}};
            ExecResult r;
            pushTensor(out, r, QStringLiteral("归一化失败"));
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- Resize ----
    {
        TaskSpec s;
        s.id = "resize";
        s.name = QStringLiteral("Resize");
        s.inputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{{"width", 224}, {"height", 224}};
        s.params = {
            {"width", QStringLiteral("宽"), "int", 224, {}},
            {"height", QStringLiteral("高"), "int", 224, {}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            if (inputs.isEmpty() || !std::holds_alternative<Tensor>(inputs[0]))
                return {false, QStringLiteral("输入不是张量"), {}};
            const auto in = onnx_convert::tensorToBuffer(std::get<Tensor>(inputs[0]));
            const auto out = resizeBuffer(in, p.value("width", 224).toInt(),
                                          p.value("height", 224).toInt());
            if (out.data.empty())
                return {false, QStringLiteral("Resize 失败（张量需为 4D 且尺寸有效）"), {}};
            ExecResult r;
            pushTensor(out, r, QStringLiteral("Resize 失败"));
            return r;
        };
        reg.add(std::move(s));
    }
}
