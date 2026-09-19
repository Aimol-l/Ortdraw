#pragma once
#include <opencv2/imgcodecs.hpp>
#include "engine/NodeExecutor.hpp"

class ImageLoadExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>&) const override {
        const QString path = params.value("path").toString();
        if (path.isEmpty())
            return {false, QStringLiteral("无法读取图片: %1").arg(path), {}};

        cv::Mat img = cv::imread(path.toStdString(), cv::IMREAD_COLOR);
        if (img.empty())
            return {false, QStringLiteral("无法读取图片: %1").arg(path), {}};

        ExecResult r;
        r.outputs.push_back(img);
        return r;
    }
};
