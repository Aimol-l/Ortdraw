#pragma once
#include <QString>
#include <variant>
#include <opencv2/core.hpp>
#include <tensorvia/core/tensor.h>

using Tensor = ::Tensor;
using NodeData = std::variant<std::monostate, cv::Mat, Tensor, double, bool>;

inline QString nodeDataName(const NodeData& d) {
    if (std::holds_alternative<cv::Mat>(d)) return "Image";
    if (std::holds_alternative<Tensor>(d)) return "Tensor";
    if (std::holds_alternative<double>(d)) return "Number";
    if (std::holds_alternative<bool>(d)) return "Bool";
    return "None";
}
