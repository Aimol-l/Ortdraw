#pragma once
#include <cstring>
#include <opencv2/imgproc.hpp>
#include "engine/NodeData.hpp"

inline cv::Mat tensorToMat(const Tensor& t) {
    Tensor c = t;
    if (c.device() != via::Device::CPU) c.to_host();
    c = c.contiguous();
    const auto shp = c.shape();
    if (shp.size() != 2) return {};
    const int rows = int(shp[0]), cols = int(shp[1]);
    cv::Mat m(rows, cols, CV_32F);
    std::memcpy(m.data, c.data(), size_t(rows) * cols * sizeof(float));
    return m;
}

inline Tensor matToTensor(const cv::Mat& m) {
    cv::Mat f; m.convertTo(f, CV_32F);
    std::vector<float> v(f.begin<float>(), f.end<float>());
    std::vector<int64_t> shape{ f.rows, f.cols };
    return Tensor(v, shape);
}

// 取出第 idx 个输入并将其视为 cv::Mat；类型不符或越界时返回 nullptr。
inline const cv::Mat* imageInput(const QVector<NodeData>& inputs, int idx = 0) {
    if (idx < 0 || idx >= inputs.size()) return nullptr;
    if (!std::holds_alternative<cv::Mat>(inputs[idx])) return nullptr;
    return &std::get<cv::Mat>(inputs[idx]);
}
