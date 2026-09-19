#pragma once
#include <opencv2/imgcodecs.hpp>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>
#include "Log.hpp"
#include "engine/NodeExecutor.hpp"

class ImageSaveExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        if (inputs.isEmpty() || !std::holds_alternative<cv::Mat>(inputs[0]))
            return {false, QStringLiteral("输入不是图像"), {}};
        const cv::Mat& mat = std::get<cv::Mat>(inputs[0]);

        QString path = params.value("path").toString();
        if (path.isEmpty()) {
            const QString dir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
                                + QStringLiteral("/Ortdraw");
            QDir().mkpath(dir);
            path = dir + QStringLiteral("/ortdraw_")
                   + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                   + QStringLiteral(".png");
        } else if (QFileInfo(path).isDir()) {
            QDir dir(path);
            path = dir.filePath(QStringLiteral("ortdraw_")
                   + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
                   + QStringLiteral(".png"));
        } else if (QFileInfo(path).suffix().isEmpty()) {
            path += QStringLiteral(".png");
        }

        const QFileInfo fi(path);
        const QString parentDir = fi.absolutePath();
        if (!parentDir.isEmpty() && !QDir(parentDir).exists())
            QDir().mkpath(parentDir);

        if (!cv::imwrite(path.toStdString(), mat))
            return {false, QStringLiteral("无法保存图片: %1").arg(path), {}};

        Log::info(QStringLiteral("保存图片：%1").arg(path));

        ExecResult r;
        r.outputs.push_back(inputs[0]);
        return r;
    }
};
