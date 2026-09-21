#pragma once
#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>

#include "node/BaseNode.hpp"
#include "engine/tasks/PreProcessRegistry.hpp"
#include "NodeManager.h"   // 换任务时经单例断边并重建端口

// 预处理节点：任务决定端口与参数；UI 依 paramDescs() 自动生成控件
class PreProcessNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString task READ task WRITE setTask NOTIFY paramsChanged)
public:
    explicit PreProcessNode(QQuickItem* parent = nullptr) : BaseNode(parent) {
        m_name = QStringLiteral("预处理");
        m_description = QStringLiteral("按任务对图像/张量做预处理（端口随任务变化）。");
        if (const TaskSpec* spec = PreProcessRegistry::instance().find(m_task)) {
            m_params = spec->defaults;
            // 构造阶段尚无连线，直接重建端口
            rebuildPorts(spec->inputs, spec->outputs);
        }
    }
    ~PreProcessNode() override = default;

    QString typeName() const override { return "PreProcess"; }
    QString category() const override { return "process"; }

    QString task() const { return m_task; }

    void setTask(const QString& id) {
        if (id == m_task) return;
        const TaskSpec* spec = PreProcessRegistry::instance().find(id);
        if (!spec) return;
        m_task = id;
        m_params = spec->defaults;
        applyTaskPorts(true);
        emit taskPortsChanged(m_task);
        emit paramsChanged();
    }

    // 任务下拉选项 [{id,name}]
    Q_INVOKABLE QVariantList taskOptions() const {
        QVariantList out;
        for (const TaskSpec& s : PreProcessRegistry::instance().all())
            out.append(QVariantMap{{"id", s.id}, {"name", s.name}});
        return out;
    }

    // 当前任务的参数描述（QML 生成控件）
    Q_INVOKABLE QVariantList paramDescs() const {
        QVariantList out;
        const TaskSpec* spec = PreProcessRegistry::instance().find(m_task);
        if (!spec) return out;
        for (const ParamDesc& d : spec->params) {
            QVariantList opts;
            for (const auto& o : d.options)
                opts.append(QVariantMap{{"value", o.first}, {"label", o.second}});
            out.append(QVariantMap{{"key", d.key}, {"label", d.label}, {"kind", d.kind},
                                   {"default", d.def}, {"options", opts}});
        }
        return out;
    }

    Q_INVOKABLE QVariantMap taskParams() const { return m_params; }

    Q_INVOKABLE void setTaskParam(const QString& k, const QVariant& v) {
        if (m_params.value(k) == v) return;
        m_params[k] = v;
        emit paramsChanged();
    }

    QVariantMap params() const override { return {{"task", m_task}, {"params", m_params}}; }

    void setParams(const QVariantMap& p) override {
        const QString id = p.contains("task") ? p.value("task").toString() : m_task;
        const TaskSpec* spec = PreProcessRegistry::instance().find(id);
        if (!spec) return;
        m_task = id;
        m_params = spec->defaults;
        const QVariantMap given = p.value("params").toMap();
        for (auto it = given.begin(); it != given.end(); ++it)
            m_params[it.key()] = it.value();
        // 读取图文件时直接重建端口（不触发断线），避免破坏已恢复的连线
        rebuildPorts(spec->inputs, spec->outputs);
        emit paramsChanged();
    }

    Q_INVOKABLE void setDisplayData(const QVariantMap&) {}

signals:
    void taskPortsChanged(const QString& task);
    void paramsChanged();

private:
    // disconnect=true：经 NodeManager 先断边再重建（运行时换任务）
    void applyTaskPorts(bool disconnect) {
        const TaskSpec* spec = PreProcessRegistry::instance().find(m_task);
        if (!spec) return;
        if (disconnect) {
            auto* nm = static_cast<NodeManager*>(NodeManager::instance());
            nm->rebuildNodePorts(this, spec->inputs, spec->outputs);
            // 无画布（如单元测试）时 rebuildNodePorts 提前返回，端口未更新 → 直接重建兜底
            if (m_input_ports.size() != spec->inputs.size() ||
                m_output_ports.size() != spec->outputs.size())
                rebuildPorts(spec->inputs, spec->outputs);
        } else {
            rebuildPorts(spec->inputs, spec->outputs);
        }
    }

    QString m_task = QStringLiteral("image_to_tensor");
    QVariantMap m_params;
};
