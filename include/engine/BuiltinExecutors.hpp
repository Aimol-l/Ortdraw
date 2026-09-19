#pragma once
#include "engine/NodeRegistry.hpp"
#include "engine/executors/ConvExecutor.hpp"
#include "engine/executors/BlurExecutor.hpp"
#include "engine/executors/EdgeDetectExecutor.hpp"
#include "engine/executors/GrayExecutor.hpp"
#include "engine/executors/ImageLoadExecutor.hpp"
#include "engine/executors/ImageSaveExecutor.hpp"
#include "engine/executors/ImageShowExecutor.hpp"
#include "engine/executors/ResizeExecutor.hpp"
#include "engine/executors/ThresholdExecutor.hpp"
#include "engine/executors/TensorExecutor.hpp"

inline void registerBuiltinExecutors() {
    auto& r = NodeRegistry::instance();
    r.registerExecutor("ImageLoad", std::make_shared<ImageLoadExecutor>());
    r.registerExecutor("ImageSave", std::make_shared<ImageSaveExecutor>());
    r.registerExecutor("ImageShow", std::make_shared<ImageShowExecutor>());
    r.registerExecutor("Resize",    std::make_shared<ResizeExecutor>());
    r.registerExecutor("Blur",      std::make_shared<BlurExecutor>());
    r.registerExecutor("Gray",      std::make_shared<GrayExecutor>());
    r.registerExecutor("Threshold", std::make_shared<ThresholdExecutor>());
    r.registerExecutor("EdgeDetect", std::make_shared<EdgeDetectExecutor>());
    r.registerExecutor("Conv",      std::make_shared<ConvExecutor>());
    r.registerExecutor("Tensor",    std::make_shared<TensorExecutor>());
}
