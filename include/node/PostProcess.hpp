#pragma once
#include <QQuickItem>
#include <QVariantList>
#include <QVariantMap>

#include "node/BaseNode.hpp"
#include "engine/tasks/PostProcessRegistry.hpp"
#include "NodeManager.h"   // 换任务时经单例断边并重建端口

// 后处理节点：任务决定端口与参数；UI 依 paramDescs() 自动生成控件，
// 并通过 displayData 承载非端口的显示数据（分类 Top-K / 检测统计）。
class PostProcessNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString task READ task WRITE setTask NOTIFY paramsChanged)
    Q_PROPERTY(QVariantMap displayData READ displayData NOTIFY displayChanged)
public:
    explicit PostProcessNode(QQuickItem* parent = nullptr) : BaseNode(parent) {
        m_name = QStringLiteral("后处理");
        min_width = 280;   // 保证参数行不越界
        m_description = QStringLiteral("按任务对张量做后处理（检测/分割/分类），端口随任务变化。");
        if (const TaskSpec* spec = PostProcessRegistry::instance().find(m_task)) {
            m_params = spec->defaults;
            rebuildPorts(spec->inputs, spec->outputs);
        }
    }
    ~PostProcessNode() override = default;

    QString typeName() const override { return "PostProcess"; }
    QString category() const override { return "process"; }

    QString task() const { return m_task; }

    void setTask(const QString& id) {
        if (id == m_task) return;
        const TaskSpec* spec = PostProcessRegistry::instance().find(id);
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
        for (const TaskSpec& s : PostProcessRegistry::instance().all())
            out.append(QVariantMap{{"id", s.id}, {"name", s.name}});
        return out;
    }

    // 当前任务的参数描述（QML 生成控件）
    Q_INVOKABLE QVariantList paramDescs() const {
        QVariantList out;
        const TaskSpec* spec = PostProcessRegistry::instance().find(m_task);
        if (!spec) return out;
        for (const ParamDesc& d : spec->params) {
            QVariantList opts;
            for (const auto& o : d.options)
                opts.append(QVariantMap{{"value", o.first}, {"label", o.second}});
            out.append(QVariantMap{{"key", d.key}, {"label", d.label}, {"kind", d.kind},
                                   {"default", d.def}, {"options", opts},
                                   {"group", d.group}, {"vecCount", d.vecCount},
                                   {"showIfKey", d.showIfKey}, {"showIfValue", d.showIfValue}});
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
        const TaskSpec* spec = PostProcessRegistry::instance().find(id);
        if (!spec) return;
        m_task = id;
        m_params = spec->defaults;
        const QVariantMap given = p.value("params").toMap();
        for (auto it = given.begin(); it != given.end(); ++it)
            m_params[it.key()] = it.value();
        // 仅当节点无连线时才重建端口（读图加载阶段）。撤销/重做经 setParams
        // 恢复参数时旧边仍指向旧端口，此时重建会删除被引用的端口造成悬垂指针。
        if(!static_cast<NodeManager*>(NodeManager::instance())->nodeHasEdges(this))
            rebuildPorts(spec->inputs, spec->outputs);
        emit paramsChanged();
    }

    // 显示通道：NodeManager 运行时经此注入；QML 绑定 displayData 渲染
    QVariantMap displayData() const { return m_display; }

    void setDisplayData(const QVariantMap& d) override {
        if (m_display == d) return;
        m_display = d;
        emit displayChanged();
    }

signals:
    void taskPortsChanged(const QString& task);
    void paramsChanged();
    void displayChanged();

private:
    // disconnect=true：经 NodeManager 先断边再重建（运行时换任务）
    void applyTaskPorts(bool disconnect) {
        const TaskSpec* spec = PostProcessRegistry::instance().find(m_task);
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

    QString m_task = QStringLiteral("yolo_detect");
    QVariantMap m_params;
    QVariantMap m_display;
};
