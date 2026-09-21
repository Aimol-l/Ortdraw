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

// Tensorvia 张量 → NodeData 输出；空张量视为失败并返回错误
inline bool pushTensor(const Tensor& t, ExecResult& r, const QString& err) {
    if (t.numel() == 0) {
        r.ok = false;
        r.error = err;
        return false;
    }
    r.outputs.push_back(NodeData{t});
    return true;
}

} // namespace preprocess_detail

inline void registerBuiltinPreProcessTasks() {
    auto& reg = PreProcessRegistry::instance();
    if (reg.find(QStringLiteral("standard"))) return;

    using namespace preprocess_detail;

    auto dtypeOf = [](const QString& v) {
        return v == QStringLiteral("fp16") ? onnx_engine::ElementType::Float16
                                           : onnx_engine::ElementType::Float32;
    };

    // ---- 标准预处理：图像 → 张量（0..1），dtype 可选 fp32/fp16，尺寸可保持或缩放 ----
    {
        TaskSpec s;
        s.id = "standard";
        s.name = QStringLiteral("标准预处理");
        s.inputs = {{QStringLiteral("图像"), DataType::Image}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{
            {"layout", "NCHW"}, {"channel", "rgb"}, {"dtype", "fp32"},
            {"norm", "unit"}, {"mean", "0.485,0.456,0.406"}, {"std", "0.229,0.224,0.225"},
            {"size", "keep"}, {"sizeWH", "0x0"},
        };
        s.params = {
            {"layout", QStringLiteral("布局"), "select", "NCHW",
             {{"NCHW", "NCHW"}, {"NHWC", "NHWC"}}, 1, 0, {}, ""},
            {"channel", QStringLiteral("通道"), "select", "rgb",
             {{"rgb", "RGB"}, {"bgr", "BGR"}}, 1, 0, {}, ""},
            {"dtype", QStringLiteral("精度"), "select", "fp32",
             {{"fp32", "fp32"}, {"fp16", "fp16"}}, 1, 0, {}, ""},
            {"norm", QStringLiteral("处理方式"), "select", "unit",
             {{"unit", QStringLiteral("0~1 (/255)")},
              {"zscore", QStringLiteral("Z-score")},
              {"pm1", QStringLiteral("[-1,1]")},
              {"minmax", QStringLiteral("逐通道 Min-Max")}}, 0, 0, {}, ""},
            {"mean", QStringLiteral("均值"), "floats", "0.485,0.456,0.406", {}, 0, 3,
             "norm", "zscore"},
            {"std", QStringLiteral("标准差"), "floats", "0.229,0.224,0.225", {}, 0, 3,
             "norm", "zscore"},
            {"size", QStringLiteral("尺寸"), "select", "keep",
             {{"keep", QStringLiteral("原尺寸")},
              {"resize", QStringLiteral("指定")}}, 2, 0, {}, ""},
            {"sizeWH", QStringLiteral("宽x高"), "size2", "0x0", {}, 2, 0, {}, ""},
        };
        s.compute = [dtypeOf](const ExecuteContext&, const QVariantMap& p,
                              const QVector<NodeData>& inputs) -> ExecResult {
            const cv::Mat* img = preprocess_detail::imageInput(inputs, 0);
            if (!img) return {false, QStringLiteral("输入不是图像"), {}};
            if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

            const QString layout = p.value("layout", "NCHW").toString();
            const QString channel = p.value("channel", "rgb").toString();
            const QString size = p.value("size", "keep").toString();
            const int channels = img->channels() == 1 ? 1 : 3;
            // sizeWH 为 "0x0"（或含 0）表示“原尺寸”
            int reqW = 0, reqH = 0;
            const QStringList wh = p.value("sizeWH", "0x0").toString().split('x');
            if (wh.size() == 2) { reqW = wh[0].toInt(); reqH = wh[1].toInt(); }
            const bool keep = (size != QStringLiteral("resize")) || reqW <= 0 || reqH <= 0;
            const int W = keep ? img->cols : reqW;
            const int H = keep ? img->rows : reqH;
            if (W <= 0 || H <= 0) return {false, QStringLiteral("目标尺寸无效"), {}};

            std::vector<int64_t> shape;
            if (layout == QStringLiteral("NHWC"))
                shape = {1, H, W, channels};
            else
                shape = {1, channels, H, W};

            // 处理方式：0~1(/255) / Z-score / [-1,1] / 逐通道 Min-Max
            const QString normOpt = p.value("norm", "unit").toString();
            const QString norm = (normOpt == QStringLiteral("div255")) ? QStringLiteral("div255")
                                 : normOpt;
            const QVector<double> mean = preprocess_detail::parseDoubles(p.value("mean").toString());
            const QVector<double> stdev = preprocess_detail::parseDoubles(p.value("std").toString());

            Tensor buf = onnx_convert::imageToTensor(
                *img, shape, dtypeOf(p.value("dtype", "fp32").toString()),
                norm, mean, stdev, channel, keep ? "keep" : "auto");
            if (buf.numel() == 0)
                return {false, QStringLiteral("标准预处理失败（通道或尺寸不匹配）"), {}};

            ExecResult r;
            pushTensor(buf, r, QStringLiteral("标准预处理失败（数据无效）"));
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- YOLO 预处理：letterbox 到 size×size，RGB，0..1 ----
    {
        TaskSpec s;
        s.id = "yolo_letterbox";
        s.name = QStringLiteral("YOLO 预处理");
        s.inputs = {{QStringLiteral("图像"), DataType::Image}};
        s.outputs = {{QStringLiteral("张量"), DataType::Tensor}};
        s.defaults = QVariantMap{{"size", 640}, {"pad", 114}, {"dtype", "fp32"}};
        s.params = {
            {"size", QStringLiteral("尺寸"), "int", 640, {}},
            {"pad", QStringLiteral("填充值"), "int", 114, {}},
            {"dtype", QStringLiteral("精度"), "select", "fp32",
             {{"fp32", "fp32"}, {"fp16", "fp16"}}},
        };
        s.compute = [dtypeOf](const ExecuteContext&, const QVariantMap& p,
                              const QVector<NodeData>& inputs) -> ExecResult {
            const cv::Mat* img = preprocess_detail::imageInput(inputs, 0);
            if (!img) return {false, QStringLiteral("输入不是图像"), {}};
            if (img->empty()) return {false, QStringLiteral("输入图像为空"), {}};

            const int size = p.value("size", 640).toInt();
            const int pad = p.value("pad", 114).toInt();
            if (size <= 0) return {false, QStringLiteral("letterbox 尺寸无效"), {}};

            cv::Mat bgr;
            if (img->channels() == 1)       cv::cvtColor(*img, bgr, cv::COLOR_GRAY2BGR);
            else if (img->channels() == 4)  cv::cvtColor(*img, bgr, cv::COLOR_BGRA2BGR);
            else                            bgr = *img;

            const double scale = std::min(double(size) / bgr.cols, double(size) / bgr.rows);
            const int nw = std::max(1, int(std::lround(bgr.cols * scale)));
            const int nh = std::max(1, int(std::lround(bgr.rows * scale)));
            cv::Mat resized;
            cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_LINEAR);

            const int top = (size - nh) / 2, bottom = size - nh - top;
            const int left = (size - nw) / 2, right = size - nw - left;
            cv::Mat canvas;
            const int pv = std::max(0, std::min(255, pad));
            cv::copyMakeBorder(resized, canvas, top, bottom, left, right,
                               cv::BORDER_CONSTANT, cv::Scalar(pv, pv, pv));

            // 统一走 imageToTensor：BGR→RGB、/255、可选 fp16
            Tensor buf = onnx_convert::imageToTensor(
                canvas, {1, 3, size, size}, dtypeOf(p.value("dtype", "fp32").toString()),
                "div255", {}, {}, "rgb", "keep");
            if (buf.numel() == 0)
                return {false, QStringLiteral("YOLO 预处理失败（尺寸或通道不匹配）"), {}};

            ExecResult r;
            pushTensor(buf, r, QStringLiteral("YOLO 预处理失败"));
            return r;
        };
        reg.add(std::move(s));
    }
}
