#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <opencv2/core/mat.hpp>
#include <print>
#include <utility>
#include <vector>
#include <QVariantList>
#include <opencv2/dnn/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include "engine/NodeData.hpp"
#include "engine/onnx/OnnxTensorConvert.hpp"
#include "engine/tasks/TaskSpec.hpp"

// 后处理任务注册表（单例，内联存储，风格同 NodeRegistry）。
// registerBuiltinPostProcessTasks() 注册内置任务，多次调用幂等。
class PostProcessRegistry {
public:
    static PostProcessRegistry& instance() { static PostProcessRegistry r; return r; }

    // 同 id 重复注册时忽略，保证多次调用 registerBuiltin* 幂等
    void add(TaskSpec s) {
        if (find(s.id)) return;
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

namespace postprocess_detail {

// 固定调色板：按类别 id 循环取色
inline const cv::Scalar& classColor(int cls) {
    static const cv::Scalar kColors[] = {
        {255, 56, 56},  {255, 157, 151}, {255, 112, 31}, {255, 178, 29},
        {207, 210, 49}, {72, 249, 251},  {146, 204, 23}, {187, 96, 224},
    };
    const int n = int(sizeof(kColors) / sizeof(kColors[0]));
    return kColors[((cls % n) + n) % n];
}

// 取第 idx 个输入为 Tensor（不做拷贝）；失败返回 nullptr
inline const Tensor* tensorInput(const QVector<NodeData>& inputs, int idx) {
    if (idx < 0 || idx >= inputs.size()) 
        return nullptr;
    if (!std::holds_alternative<Tensor>(inputs[idx])) 
        return nullptr;
    const Tensor& t = std::get<Tensor>(inputs[idx]);
    
    return t.numel() == 0 ? nullptr : &t;
}

// 取第 idx 个输入为非空图像；失败返回 nullptr
inline const cv::Mat* imageInputAt(const QVector<NodeData>& inputs, int idx = 0) {
    if (idx < 0 || idx >= inputs.size()) return nullptr;
    if (!std::holds_alternative<cv::Mat>(inputs[idx])) return nullptr;
    const cv::Mat& m = std::get<cv::Mat>(inputs[idx]);
    return m.empty() ? nullptr : &m;
}

// 按 (channel, index) 读取 host 张量元素
inline float readElement(const Tensor& host, std::size_t idx) {
    return onnx_convert::tensorElement(host, idx);
}

// 几何元信息（与预处理端 1x8 布局一致）
struct GeometryMeta {
    int mode = 0;                       // 0=resize，1=letterbox
    float origW = 0.0f, origH = 0.0f;
    float netW = 0.0f, netH = 0.0f;
    float scale = 1.0f;
    float padX = 0.0f, padY = 0.0f;
};

// 解析元信息张量：要求至少 8 个元素，取前 8 个
inline bool parseMeta(const Tensor& b, GeometryMeta& m) {
    if (b.numel() < 8) return false;
    m.mode = int(std::lround(readElement(b, 0)));
    m.origW = readElement(b, 1);
    m.origH = readElement(b, 2);
    m.netW = readElement(b, 3);
    m.netH = readElement(b, 4);
    m.scale = readElement(b, 5);
    m.padX = readElement(b, 6);
    m.padY = readElement(b, 7);
    return m.origW > 0.0f && m.origH > 0.0f;
}

// 用元信息把网络坐标框逆变换回原图坐标并 clamp 到原图边界
inline cv::Rect2f mapBoxToOriginal(const cv::Rect2f& box, const GeometryMeta& m) {
    float x0 = box.x, y0 = box.y, x1 = box.x + box.width, y1 = box.y + box.height;
    if (m.mode == 1) {
        if (m.scale > 0.0f) {
            x0 = (x0 - m.padX) / m.scale;
            y0 = (y0 - m.padY) / m.scale;
            x1 = (x1 - m.padX) / m.scale;
            y1 = (y1 - m.padY) / m.scale;
        }
    } else {
        const float sx = m.netW > 0.0f ? m.origW / m.netW : 1.0f;
        const float sy = m.netH > 0.0f ? m.origH / m.netH : 1.0f;
        x0 *= sx; x1 *= sx;
        y0 *= sy; y1 *= sy;
    }
    x0 = std::clamp(x0, 0.0f, m.origW);
    x1 = std::clamp(x1, 0.0f, m.origW);
    y0 = std::clamp(y0, 0.0f, m.origH);
    y1 = std::clamp(y1, 0.0f, m.origH);
    return cv::Rect2f(x0, y0, std::max(0.0f, x1 - x0), std::max(0.0f, y1 - y0));
}

// 把网络空间掩码还原到原图：letterbox 先裁掉填充区，再缩放到原图尺寸
inline cv::Mat maskToOriginal(const cv::Mat& netMask, const GeometryMeta& m) {
    const int ow = std::max(1, int(std::lround(m.origW)));
    const int oh = std::max(1, int(std::lround(m.origH)));
    cv::Mat src = netMask;
    if (m.mode == 1 && m.scale > 0.0f) {
        const int x0 = int(std::lround(m.padX));
        const int y0 = int(std::lround(m.padY));
        const int cw = int(std::lround(m.scale * m.origW));
        const int ch = int(std::lround(m.scale * m.origH));
        cv::Rect roi = cv::Rect(x0, y0, cw, ch) & cv::Rect(0, 0, netMask.cols, netMask.rows);
        if (roi.width <= 0 || roi.height <= 0)
            return cv::Mat::zeros(oh, ow, CV_8U);
        src = netMask(roi);
    }
    cv::Mat out;
    cv::resize(src, out, cv::Size(ow, oh), 0, 0, cv::INTER_NEAREST);
    return out;
}

inline float sigmoid(float x) { return 1.0f / (1.0f + std::exp(-x)); }

// 一个候选检测框（xyxy，网络输入坐标）
struct Detection {
    cv::Rect2f box;
    float score = 0.0f;
    int cls = 0;
    std::vector<float> coeff;   // 分割掩码系数（yolo_detect 为空）
};

// YOLO 检测头形状解析：支持 [1, 4+nc, N]（channels-first）与 [1, N, 4+nc]。
// 计划给出的判据是 shape[1] < shape[2] 时为 channels-first；但 N 可能小于 4+nc
// （如合成测试的 [1,84,1]），故先用「哪一维才可能是 4+nc（>=5）」判定，再回退到大小比较。
// channelsFirst=true 时：C=shape[1], N=shape[2]，元素偏移 = c*N + n；
// channelsFirst=false 时：N=shape[1], C=shape[2]，元素偏移 = n*C + c。
inline bool parseDetectHead(const Tensor& b, bool& channelsFirst,
                            int64_t& C, int64_t& N) {
    const auto shape = b.shape();
    if (shape.size() != 3 || shape[0] != 1) return false;
    const int64_t d1 = shape[1], d2 = shape[2];
    if (d1 <= 0 || d2 <= 0) return false;

    if (d1 >= 5 && d2 < 5) {
        channelsFirst = true;   // [1, 4+nc, N]，N 很小
    } else if (d1 < 5 && d2 >= 5) {
        channelsFirst = false;  // [1, N, 4+nc]，N 很小
    } else if (d1 < d2) {
        channelsFirst = true;   // [1, 4+nc, N]
    } else {
        channelsFirst = false;  // [1, N, 4+nc]
    }
    C = channelsFirst ? d1 : d2;
    N = channelsFirst ? d2 : d1;
    return C >= 5;
}

// 统一检测头为 [1, M, 4+n]（channels-last）；若原为 [1, 4+n, M] 则转置一次。
inline bool unifyDetectHead(const Tensor& host, Tensor& unified) {
    bool cf = false;
    int64_t C = 0, N = 0;
    if (!parseDetectHead(host, cf, C, N)) return false;
    if (!cf) {
        unified = host;
        return true;
    }
    Tensor t = host.permute({0, 2, 1});   // 视图 → 连续化
    t = t.contiguous();
    t.to_host();
    unified = std::move(t);
    return true;
}

// 解析检测头（含可选掩码系数范围 [coeffBegin, C)），过滤 conf 后做 NMS
inline bool decodeDetections(const Tensor& b, float conf, float iou,
                             int maxBoxes, std::vector<Detection>& out) {
    // 调用方已通过 unifyDetectHead 统一为 [1, M, 4+n]
    const auto shape = b.shape();
    if (shape.size() != 3 || shape[0] != 1) return false;
    const int64_t N = shape[1], C = shape[2];
    if (N <= 0 || C < 5) return false;

    auto at = [&](int64_t c, int64_t n) -> float {
        return readElement(b, std::size_t(n) * std::size_t(C) + std::size_t(c));
    };

    std::vector<Detection> cands;
    cands.reserve(std::size_t(N));
    for (int64_t n = 0; n < N; ++n) {
        const float cx = at(0, n), cy = at(1, n), w = at(2, n), h = at(3, n);
        float best = 0.0f;
        int bestCls = -1;
        for (int64_t c = 4; c < C; ++c) {
            const float s = at(c, n);
            if (s > best) { best = s; bestCls = int(c - 4); }
        }
        if (bestCls < 0 || best < conf) continue;
        Detection d;
        d.box = cv::Rect2f(cx - w * 0.5f, cy - h * 0.5f, w, h);
        d.score = best;
        d.cls = bestCls;
        cands.push_back(std::move(d));
    }

    // 类内 NMS：用 OpenCV dnn 的 NMSBoxesBatched（同一类别内抑制重叠框；
    // 不同类别的重叠框都会保留，这是 YOLO 的正确行为）
    out.clear();
    if (cands.empty()) return true;

    std::vector<cv::Rect2d> boxes;
    std::vector<float> scores;
    std::vector<int> classIds;
    boxes.reserve(cands.size());
    scores.reserve(cands.size());
    classIds.reserve(cands.size());
    for (const Detection& d : cands) {
        boxes.emplace_back(d.box.x, d.box.y, d.box.width, d.box.height);
        scores.push_back(d.score);
        classIds.push_back(d.cls);
    }

    std::vector<int> keep;
    cv::dnn::NMSBoxesBatched(boxes, scores, classIds, conf, iou, keep, 1.0f, maxBoxes);
    out.reserve(keep.size());
    for (int i : keep) out.push_back(cands[std::size_t(i)]);
    return true;
}

// 在画布上画框 + "<cls> <score两位小数>"
inline void drawDetections(cv::Mat& canvas, const std::vector<Detection>& dets,bool drawScore, int lineWidth) {
    const int lw = lineWidth > 0 ? lineWidth : 1;
    for (const Detection& d : dets) {
        const cv::Scalar& col = classColor(d.cls);
        cv::rectangle(canvas,
                      cv::Point(int(std::lround(d.box.x)), int(std::lround(d.box.y))),
                      cv::Point(int(std::lround(d.box.x + d.box.width)),
                                int(std::lround(d.box.y + d.box.height))),
                      col, lw);
        std::string text = std::format("{}:{:.2f}",d.cls,d.score);
        if(lw>0)
            cv::putText(canvas, text,
                    cv::Point(int(std::lround(d.box.x)), std::max(12, int(std::lround(d.box.y)) - 4)),
                    cv::FONT_HERSHEY_SIMPLEX, 1, col, 1, cv::LINE_AA);
    }
}

// 分割原型 [1, nm, mh, mw]（或 [nm, mh, mw]）→ 逐元素访问
struct ProtoView {
    const Tensor* host = nullptr;
    bool chFirst = true;
    int64_t nm = 0, mh = 0, mw = 0;

    static bool make(const Tensor& b, ProtoView& v) {
        v.host = &b;
        const auto shape = b.shape();
        if (shape.size() == 4) {
            if (shape[0] != 1) return false;
            v.chFirst = shape[1] < shape[3];
            v.nm = v.chFirst ? shape[1] : shape[3];
            v.mh = v.chFirst ? shape[2] : shape[1];
            v.mw = v.chFirst ? shape[3] : shape[2];
        } else if (shape.size() == 3) {
            v.chFirst = true;
            v.nm = shape[0];
            v.mh = shape[1];
            v.mw = shape[2];
        } else {
            return false;
        }
        return v.nm > 0 && v.mh > 0 && v.mw > 0;
    }

    float at(int64_t k, int64_t y, int64_t x) const {
        std::size_t idx;
        if (host->shape().size() == 3)
            idx = std::size_t((k * mh + y) * mw + x);
        else
            idx = chFirst ? std::size_t((k * mh + y) * mw + x)
                          : std::size_t(((y * mw + x) * nm) + k);
        return readElement(*host, idx);
    }
};

// 生成单个框的掩码（sigmoid(系数·原型) > maskThr），返回 netSize 的 CV_8U（0/255）
inline cv::Mat buildMask(const std::vector<float>& coeff, const ProtoView& pv,
                         float maskThr, cv::Size netSize) {
    cv::Mat prob(int(pv.mh), int(pv.mw), CV_32F);
    for (int64_t y = 0; y < pv.mh; ++y)
        for (int64_t x = 0; x < pv.mw; ++x) {
            float sum = 0.0f;
            const int64_t k = std::min<int64_t>(pv.nm, int64_t(coeff.size()));
            for (int64_t c = 0; c < k; ++c) sum += coeff[std::size_t(c)] * pv.at(c, y, x);
            prob.at<float>(int(y), int(x)) = sigmoid(sum);
        }
    cv::Mat bin;
    cv::compare(prob, maskThr, bin, cv::CMP_GT);
    cv::Mat resized;
    cv::resize(bin, resized, netSize, 0, 0, cv::INTER_NEAREST);
    return resized;
}

} // namespace postprocess_detail

inline void registerBuiltinPostProcessTasks() {
    auto& reg = PostProcessRegistry::instance();
    if (reg.find(QStringLiteral("yolo_detect"))) return;

    using namespace postprocess_detail;

    // ---- YOLO 检测 ----
    {
        TaskSpec s;
        s.id = "yolo_detect";
        s.name = QStringLiteral("YOLO 检测");
        s.inputs = {{QStringLiteral("检测"), DataType::Tensor},
                    {QStringLiteral("原图"), DataType::Image},
                    {QStringLiteral("元信息"), DataType::Tensor}};
        s.outputs = {{QStringLiteral("图像"), DataType::Image}};
        s.defaults = QVariantMap{{"conf", 0.25}, {"iou", 0.45},
                                 {"maxBoxes", 300}, {"drawScore", true}, {"lineWidth", 2}};
        s.params = {
            {"conf", QStringLiteral("置信度"), "float", 0.25, {}},
            {"iou", QStringLiteral("IoU"), "float", 0.45, {}},
            {"maxBoxes", QStringLiteral("最大框数"), "int", 300, {}},
            {"drawScore", QStringLiteral("画分数"), "bool", true, {}},
            {"lineWidth", QStringLiteral("线宽"), "int", 2, {}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            const Tensor* head = tensorInput(inputs, 0); // [batch,4+n,M] or [batch,M,4+n]
            const cv::Mat* img = imageInputAt(inputs, 1);
            const Tensor* metaIn = tensorInput(inputs, 2);
            if (!head || !img || !metaIn)
                return {false, QStringLiteral("需要 检测/原图/元信息 三个输入"), {}};

            Tensor headHost, metaHost;
            if (!onnx_convert::toHostTensor(*head, headHost)
                || !unifyDetectHead(headHost, headHost))
                return {false, QStringLiteral("YOLO 输出形状无法解析"), {}};
            if (!onnx_convert::toHostTensor(*metaIn, metaHost))
                return {false, QStringLiteral("元信息张量无效"), {}};

            GeometryMeta meta;
            if (!parseMeta(metaHost, meta))
                return {false, QStringLiteral("元信息张量无效"), {}};

            // 约束：元信息记录的原图尺寸必须与输入原图一致
            if (int(std::lround(meta.origW)) != img->cols || int(std::lround(meta.origH)) != img->rows)
                return {false, QStringLiteral("元信息中的原图尺寸与输入原图不一致"), {}};

            const float iou      = p.value("iou", 0.45).toFloat();
            const int lineWidth  = p.value("lineWidth", 2).toInt();
            const float conf     = p.value("conf", 0.25).toFloat();
            const bool drawScore = p.value("drawScore", true).toBool();
            const int maxBoxes   = std::max(1, p.value("maxBoxes", 300).toInt());

            std::vector<Detection> dets;
            if (!decodeDetections(headHost, conf, iou, maxBoxes, dets)){
                return {false, QStringLiteral("YOLO 输出形状无法解析"), {}};
            }
            // 网络坐标 → 原图坐标
            for (Detection& d : dets) d.box = mapBoxToOriginal(d.box, meta);

            cv::Mat canvas = img->clone();
            drawDetections(canvas, dets, drawScore, lineWidth);

            ExecResult r;
            r.outputs.push_back(canvas);
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- YOLO 分割 ----
    {
        TaskSpec s;
        s.id = "yolo_segment";
        s.name = QStringLiteral("YOLO 分割");
        s.inputs = {{QStringLiteral("检测"), DataType::Tensor},
                    {QStringLiteral("原型"), DataType::Tensor},
                    {QStringLiteral("原图"), DataType::Image},
                    {QStringLiteral("元信息"), DataType::Tensor}};
        s.outputs = {{QStringLiteral("图像"), DataType::Image}};
        s.defaults = QVariantMap{{"conf", 0.25}, {"maskThr", 0.5},
                                 {"alpha", 0.45}, {"maxBoxes", 300}};
        s.params = {
            {"conf", QStringLiteral("置信度"), "float", 0.25, {}},
            {"maskThr", QStringLiteral("掩码阈值"), "float", 0.5, {}},
            {"alpha", QStringLiteral("透明度"), "float", 0.45, {}},
            {"maxBoxes", QStringLiteral("最大框数"), "int", 300, {}},
        };
        s.compute = [](const ExecuteContext&, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            const Tensor* headIn = tensorInput(inputs, 0);
            const Tensor* protoIn = tensorInput(inputs, 1);
            const cv::Mat* img = imageInputAt(inputs, 2);
            const Tensor* metaIn = tensorInput(inputs, 3);
            if (!headIn || !protoIn || !img || !metaIn)
                return {false, QStringLiteral("需要 检测/原型/原图/元信息 四个输入"), {}};

            Tensor head, protos, metaHost;
            if (!onnx_convert::toHostTensor(*headIn, head)
                || !unifyDetectHead(head, head))
                return {false, QStringLiteral("YOLO 分割输出形状无法解析"), {}};
            if (!onnx_convert::toHostTensor(*protoIn, protos) ||
                !onnx_convert::toHostTensor(*metaIn, metaHost))
                return {false, QStringLiteral("需要 检测/原型/原图/元信息 四个输入"), {}};

            GeometryMeta meta;
            if (!parseMeta(metaHost, meta))
                return {false, QStringLiteral("元信息张量无效"), {}};
            // 约束：元信息记录的原图尺寸必须与输入原图一致
            if (int(std::lround(meta.origW)) != img->cols
                || int(std::lround(meta.origH)) != img->rows)
                return {false, QStringLiteral("元信息中的原图尺寸与输入原图不一致"), {}};

            ProtoView pv;
            if (!ProtoView::make(protos, pv))
                return {false, QStringLiteral("分割原型形状无法解析"), {}};

            const float conf = p.value("conf", 0.25).toFloat();
            const float maskThr = p.value("maskThr", 0.5).toFloat();
            const float alpha = p.value("alpha", 0.45).toFloat();
            const int maxBoxes = std::max(1, p.value("maxBoxes", 300).toInt());

            // 检测头已统一为 [1, M, 4+nc+nm]
            const auto hs = head.shape();
            if (hs.size() != 3 || hs[0] != 1)
                return {false, QStringLiteral("YOLO 分割输出形状无法解析"), {}};
            const int64_t N = hs[1], C = hs[2];
            if (C < 4 + pv.nm)
                return {false, QStringLiteral("YOLO 分割输出形状无法解析"), {}};

            // 逐框读取掩码系数（channels-last 索引）
            auto at = [&](int64_t c, int64_t n) -> float {
                return readElement(head, std::size_t(n) * std::size_t(C) + std::size_t(c));
            };
            const int64_t nc = C - 4 - pv.nm;

            std::vector<Detection> cands;
            for (int64_t n = 0; n < N; ++n) {
                const float cx = at(0, n), cy = at(1, n), w = at(2, n), h = at(3, n);
                float best = 0.0f;
                int bestCls = -1;
                for (int64_t c = 4; c < 4 + nc; ++c) {
                    const float sc = at(c, n);
                    if (sc > best) { best = sc; bestCls = int(c - 4); }
                }
                if (bestCls < 0 || best < conf) continue;
                Detection d;
                d.box = cv::Rect2f(cx - w * 0.5f, cy - h * 0.5f, w, h);
                d.score = best;
                d.cls = bestCls;
                d.coeff.resize(std::size_t(pv.nm));
                for (int64_t k = 0; k < pv.nm; ++k)
                    d.coeff[std::size_t(k)] = at(4 + nc + k, n);
                cands.push_back(std::move(d));
            }
            std::sort(cands.begin(), cands.end(),
                      [](const Detection& a, const Detection& b) { return a.score > b.score; });
            std::vector<Detection> dets;
            for (const Detection& d : cands) {
                if (int(dets.size()) >= maxBoxes) break;
                dets.push_back(d);
            }

            cv::Mat canvas = img->clone();
            // 首版复杂度控制：仅对第一个（最高分）框生成掩码并半透明叠加；
            // 其余框只画检测框。后续可对每个框重复此流程。
            if (!dets.empty()) {
                const int netW = std::max(1, int(std::lround(meta.netW > 0.0f ? meta.netW : meta.origW)));
                const int netH = std::max(1, int(std::lround(meta.netH > 0.0f ? meta.netH : meta.origH)));
                const cv::Mat netMask = buildMask(dets[0].coeff, pv, maskThr, cv::Size(netW, netH));
                const cv::Mat mask = maskToOriginal(netMask, meta);
                cv::Mat overlay = canvas.clone();
                if (mask.size() == canvas.size())
                    overlay.setTo(classColor(dets[0].cls), mask);
                cv::addWeighted(overlay, alpha, canvas, 1.0 - alpha, 0.0, canvas);
            }
            // 网络坐标 → 原图坐标后画框
            for (Detection& d : dets) d.box = mapBoxToOriginal(d.box, meta);
            drawDetections(canvas, dets, true, 2);

            ExecResult r;
            r.outputs.push_back(canvas);
            return r;
        };
        reg.add(std::move(s));
    }

    // ---- 分类 ----
    {
        TaskSpec s;
        s.id = "classify";
        s.name = QStringLiteral("分类");
        s.inputs = {{QStringLiteral("input0"), DataType::Tensor}};
        s.outputs = {{QStringLiteral("类别 id"), DataType::Number}};
        s.defaults = QVariantMap{{"topk", 5}};
        s.params = {
            {"topk", QStringLiteral("Top-K"), "int", 5, {}},
        };
        s.compute = [](const ExecuteContext& ctx, const QVariantMap& p,
                       const QVector<NodeData>& inputs) -> ExecResult {
            const Tensor* in = tensorInput(inputs, 0);
            if (!in) return {false, QStringLiteral("输入不是张量"), {}};

            Tensor buf;
            if (!onnx_convert::toHostTensor(*in, buf))
                return {false, QStringLiteral("输入张量为空"), {}};

            const std::size_t n = buf.numel();
            if (n == 0) return {false, QStringLiteral("输入张量为空"), {}};

            std::vector<float> logits(n);
            float mx = -std::numeric_limits<float>::infinity();
            for (std::size_t i = 0; i < n; ++i) {
                logits[i] = readElement(buf, i);
                mx = std::max(mx, logits[i]);
            }
            double sum = 0.0;
            for (std::size_t i = 0; i < n; ++i) {
                logits[i] = std::exp(logits[i] - mx);
                sum += logits[i];
            }
            if (sum <= 0.0) return {false, QStringLiteral("分类 softmax 失败"), {}};
            for (float& v : logits) v = float(v / sum);

            std::size_t argmax = 0;
            for (std::size_t i = 1; i < n; ++i)
                if (logits[i] > logits[argmax]) argmax = i;

            // Top-K（按分数降序，仅显示用）
            const int topk = std::max(1, p.value("topk", 5).toInt());
            std::vector<std::size_t> order(n);
            for (std::size_t i = 0; i < n; ++i) order[i] = i;
            const int kk = std::min<int>(topk, int(n));
            std::partial_sort(order.begin(), order.begin() + kk, order.end(),
                              [&](std::size_t a, std::size_t b) { return logits[a] > logits[b]; });

            if (ctx.display) {
                QVariantList topkList;
                for (int i = 0; i < kk; ++i)
                    topkList.append(QVariantMap{{"id", int(order[std::size_t(i)])},
                                                {"score", double(logits[order[std::size_t(i)]])}});
                ctx.display(QVariantMap{{"top1", int(argmax)},
                                        {"top1Score", double(logits[argmax])},
                                        {"topk", topkList}});
            }

            ExecResult r;
            r.outputs.push_back(double(argmax));
            return r;
        };
        reg.add(std::move(s));
    }
}
