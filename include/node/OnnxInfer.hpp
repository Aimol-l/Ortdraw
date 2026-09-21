#pragma once
#include <QQuickItem>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <string>

#include "node/BaseNode.hpp"
#include "onnx_engine/runtime.hpp"
#include "NodeManager.h"   // 换模型时经单例断边并重建端口

// ONNX 推理节点：按模型元数据动态生成张量端口（名=模型输入/输出名）
class OnnxInferNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath NOTIFY paramsChanged)
    Q_PROPERTY(QString device READ device WRITE setDevice NOTIFY paramsChanged)
    Q_PROPERTY(int threads READ threads WRITE setThreads NOTIFY paramsChanged)
    Q_PROPERTY(QString ioText READ ioText NOTIFY paramsChanged)
    Q_PROPERTY(QString modelError READ modelError NOTIFY paramsChanged)
public:
    explicit OnnxInferNode(QQuickItem* parent = nullptr) : BaseNode(parent) {
        m_name = QStringLiteral("ONNX 推理");
        m_description = QStringLiteral("加载 ONNX 模型，端口按模型元数据生成（输入/输出均为张量）。");
    }
    ~OnnxInferNode() override = default;

    QString typeName() const override { return "OnnxInfer"; }
    QString category() const override { return "math"; }

    QString modelPath() const { return m_path; }

    void setModelPath(const QString& p) {
        if (p == m_path) return;
        m_path = p;
        loadMetadata(true);
        emit paramsChanged();
    }

    QString device() const { return m_device; }

    void setDevice(const QString& d) {
        if (d == m_device) return;
        m_device = d;
        emit paramsChanged();
    }

    int threads() const { return m_threads; }

    void setThreads(int t) {
        if (t == m_threads) return;
        m_threads = t;
        emit paramsChanged();
    }

    // 每行 "name: dtype [shape]"，-1 显示为「动态」
    QString ioText() const { return m_ioText; }
    // 载入失败信息
    QString modelError() const { return m_modelError; }

    Q_INVOKABLE void reloadModel() {
        loadMetadata(true);
        emit paramsChanged();
    }

    QVariantMap params() const override {
        return {{"modelPath", m_path}, {"device", m_device}, {"threads", m_threads}};
    }

    // 恢复参数并重建端口（加载阶段直接 rebuildPorts，不发请求）
    void setParams(const QVariantMap& p) override {
        m_device = p.value("device", m_device).toString();
        m_threads = p.value("threads", m_threads).toInt();
        const QString path = p.value("modelPath", m_path).toString();
        if (path != m_path || m_input_ports.isEmpty()) {
            m_path = path;
            loadMetadata(false);
        }
        emit paramsChanged();
    }

    Q_INVOKABLE void setDisplayData(const QVariantMap&) {}

signals:
    void modelPortsChanged(const QString& path);   // 通知 NodeManager 断边后重建端口
    void paramsChanged();

private:
    static QString describeTensor(const onnx_engine::TensorInfo& t) {
        QStringList dims;
        for (int64_t d : t.shape)
            dims << (d < 0 ? QStringLiteral("动态") : QString::number(d));
        return QStringLiteral("%1: %2 [%3]")
            .arg(QString::fromStdString(t.name),
                 QString::fromLatin1(onnx_engine::elementTypeName(t.type)),
                 dims.join(QStringLiteral(", ")));
    }

    // disconnect=true：先断边再重建（运行时换模型）；false：直接重建（读图恢复）
    void loadMetadata(bool disconnect) {
        QVector<PortSpec> ins, outs;
        if (m_path.isEmpty()) {
            m_ioText.clear();
            m_modelError.clear();
            applyPorts(ins, outs, disconnect);
            return;
        }

        onnx_engine::SessionOptions o;
        o.device = m_device == QStringLiteral("cpu")  ? onnx_engine::Device::CPU
                 : m_device == QStringLiteral("cuda") ? onnx_engine::Device::CUDA
                                                      : onnx_engine::Device::Auto;
        o.intraThreads = m_threads;

        std::string err;
        const onnx_engine::ModelInfo info =
            onnx_engine::Runtime::instance().modelInfo(m_path.toStdString(), o, err);
        if (!err.empty()) {
            m_modelError = QString::fromStdString(err);
            m_ioText.clear();
            applyPorts(ins, outs, disconnect);
            return;
        }

        m_modelError.clear();
        QStringList lines;
        for (const auto& t : info.inputs) {
            if (!t.isTensor) continue;
            ins.push_back({QString::fromStdString(t.name), DataType::Tensor});
            lines << describeTensor(t);
        }
        for (const auto& t : info.outputs) {
            if (!t.isTensor) continue;
            outs.push_back({QString::fromStdString(t.name), DataType::Tensor});
            lines << describeTensor(t);
        }
        m_ioText = lines.join(QLatin1Char('\n'));
        applyPorts(ins, outs, disconnect);
        emit modelPortsChanged(m_path);
    }

    // 与 PreProcessNode::applyTaskPorts 相同：经 NodeManager 断边；无画布时兜底直建
    void applyPorts(const QVector<PortSpec>& ins, const QVector<PortSpec>& outs, bool disconnect) {
        if (disconnect) {
            auto* nm = static_cast<NodeManager*>(NodeManager::instance());
            nm->rebuildNodePorts(this, ins, outs);
            if (m_input_ports.size() != ins.size() || m_output_ports.size() != outs.size())
                rebuildPorts(ins, outs);
        } else {
            rebuildPorts(ins, outs);
        }
    }

    QString m_path;
    QString m_device = QStringLiteral("auto");
    int m_threads = 0;
    QString m_ioText;
    QString m_modelError;
};
