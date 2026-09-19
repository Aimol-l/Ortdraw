#pragma once
#include <opencv2/imgproc.hpp>
#include "engine/executors/ImageConvert.hpp"

class EdgeDetectExecutor : public NodeExecutor {
public:
    static constexpr int kSobel = 0;
    static constexpr int kScharr = 1;
    static constexpr int kLaplacian = 2;
    static constexpr int kCanny = 3;

    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>& inputs) const override {
        const cv::Mat* img = imageInput(inputs);
        if (!img) return {false, QStringLiteral("输入不是图像"), {}};

        cv::Mat g;
        if (img->channels() == 1) g = *img;
        else cv::cvtColor(*img, g, cv::COLOR_BGR2GRAY);

        const int method = params.value("method", kCanny).toInt();
        int k = params.value("kernel", 3).toInt();
        if (k < 3 || k > 7) k = 3;
        if (k % 2 == 0) ++k;          // 核大小须为奇数

        cv::Mat out;
        switch (method) {
        case kSobel: {
            cv::Mat gx, gy, mag;
            cv::Sobel(g, gx, CV_16S, 1, 0, k);
            cv::Sobel(g, gy, CV_16S, 0, 1, k);
            cv::Mat fx, fy;
            gx.convertTo(fx, CV_32F);
            gy.convertTo(fy, CV_32F);
            cv::magnitude(fx, fy, mag);
            cv::convertScaleAbs(mag, out);
            break;
        }
        case kScharr: {
            cv::Mat gx, gy, mag;
            cv::Sobel(g, gx, CV_16S, 1, 0, -1);   // -1 => Scharr
            cv::Sobel(g, gy, CV_16S, 0, 1, -1);
            cv::Mat fx, fy;
            gx.convertTo(fx, CV_32F);
            gy.convertTo(fy, CV_32F);
            cv::magnitude(fx, fy, mag);
            cv::convertScaleAbs(mag, out);
            break;
        }
        case kLaplacian: {
            cv::Mat lap;
            cv::Laplacian(g, lap, CV_16S, k);
            cv::convertScaleAbs(lap, out);
            break;
        }
        default: {                                 // Canny
            const int low = params.value("low", 100).toInt();
            const int high = params.value("high", 200).toInt();
            cv::Canny(g, out, low, high, k);
            break;
        }
        }

        ExecResult r;
        r.outputs.push_back(out);
        return r;
    }
};
