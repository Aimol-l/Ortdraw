#pragma once
#include <QFileInfo>
#include <QLocale>
#include <opencv2/videoio.hpp>
#include "node/BaseNode.hpp"

// 视频输入：从本地视频文件按帧号取一帧，输出 Image（BGR 8UC3）。
// 播放进度（frame）由 QML 播放条推进，不进撤销栈；可撤销参数只有 path/fps/loop。
class VideoInputNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY infoChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY infoChanged)
    Q_PROPERTY(bool videoValid READ videoValid NOTIFY infoChanged)
public:
    QString typeName() const override { return "VideoInput"; }
    QString category() const override { return "input"; }

    QString path() const { return m_path; }
    void setPath(const QString& p) {
        if (m_path == p) return;
        m_path = p;
        m_frame = 0;                     // 换文件从第 0 帧开始
        refreshInfo();
        emit pathChanged();
        emit frameChanged();
    }

    int fps() const { return m_fps; }
    void setFps(int v) {
        if (!(v >= 1)) v = 1;            // 同时挡住 NaN
        if (v > 120) v = 120;
        if (m_fps == v) return;
        m_fps = v;
        emit fpsChanged();
    }

    bool loop() const { return m_loop; }
    void setLoop(bool v) {
        if (m_loop == v) return;
        m_loop = v;
        emit loopChanged();
    }

    int frame() const { return m_frame; }
    void setFrame(int v) {
        if (v < 0) v = 0;
        if (m_frame == v) return;
        m_frame = v;
        emit frameChanged();
    }

    int frameCount() const { return m_frameCount; }
    bool videoValid() const { return m_valid; }
    QString infoText() const { return m_infoText; }

    QVariantMap params() const override {
        return {{"path", m_path}, {"fps", m_fps}, {"loop", m_loop}, {"frame", m_frame}};
    }
    void setParams(const QVariantMap& p) override {
        if (p.contains("path")) setPath(p.value("path").toString());   // 会把 frame 归零
        if (p.contains("fps")) setFps(p.value("fps").toInt());
        if (p.contains("loop")) setLoop(p.value("loop").toBool());
        if (p.contains("frame")) setFrame(p.value("frame").toInt());   // 必须放在 path 之后
    }

    VideoInputNode(QQuickItem* parent = nullptr) : BaseNode(parent) {
        m_name = QStringLiteral("视频输入");
        m_description = QStringLiteral("读取视频文件的一帧，输出图像；可按帧率播放。");
        min_width = 240;
        m_output_ports.push_back(new Port(QStringLiteral("输出"), PortType::Output,
                                          PortDataType::Image, QPointF(0, 0), this));
        refreshInfo();
    }

private:
    // 打开一次读元信息；不持有 capture（执行器自己缓存）
    void refreshInfo() {
        m_frameCount = 0;
        m_valid = false;
        m_infoText.clear();
        if (m_path.isEmpty()) { emit infoChanged(); return; }

        const QFileInfo fi(m_path);
        if (!fi.exists() || !fi.isFile()) {
            m_infoText = QStringLiteral("文件不存在");
            emit infoChanged();
            return;
        }
        cv::VideoCapture cap(m_path.toStdString());
        if (!cap.isOpened()) {
            m_infoText = QStringLiteral("无法打开（缺解码器？）");
            emit infoChanged();
            return;
        }
        const int w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        const int h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        const double fps = cap.get(cv::CAP_PROP_FPS);
        const int n = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        m_frameCount = n;
        m_valid = true;
        m_infoText = QStringLiteral("%1 × %2  ·  %3 FPS  ·  %4 帧  ·  %5")
                         .arg(w).arg(h)
                         .arg(fps, 0, 'f', 1)
                         .arg(n > 0 ? QString::number(n) : QStringLiteral("?"))
                         .arg(QLocale().formattedDataSize(fi.size()));
        emit infoChanged();
    }

signals:
    void pathChanged();
    void fpsChanged();
    void loopChanged();
    void frameChanged();
    void infoChanged();

private:
    QString m_path;
    int m_fps = 25;
    bool m_loop = true;
    int m_frame = 0;
    int m_frameCount = 0;
    bool m_valid = false;
    QString m_infoText;
};
