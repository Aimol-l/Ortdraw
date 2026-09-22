#pragma once
#include <algorithm>
#include <print>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include <QSizeF>
#include <QLineF>
#include <QPair>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "PaintBoard.h"
#include "Settings.h"
#include "Log.hpp"
#include <limits>
#include "utils/DAGraph.hpp"
#include "command/AddNode.hpp"
#include "command/AddEdge.hpp"
#include "command/RemoveEdge.hpp"
#include "command/RemoveNode.hpp"
#include "command/MoveNodeCMD.hpp"
#include "command/ResizeNodeCMD.hpp"
#include "command/ChangeParamsCMD.hpp"
#include "command/CmdManager.hpp"
#include "utils/Snapshot.hpp"
#include "engine/GraphExecutor.hpp"
#include "engine/ImageStore.hpp"
#include "engine/NodeStatus.hpp"
#include "ExecQueueModel.hpp"
#include "QueueFilterProxyModel.hpp"
#include "utils/QueueGroups.hpp"


class NodeManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
private:
    CmdManager m_cmd_manager;
    QPointer<PaintBoard> m_paint_board;
    BaseNode* m_selected_node = nullptr;
    Port* m_link_src = nullptr;
    Port* m_link_target = nullptr;
    bool m_link_from_input = false;
    GraphExecutor m_executor;
    ExecQueueModel m_exec_queue;
    QueueFilterProxyModel m_queue_proxy;
    QVariantList m_group_summary;          // [{id,name,color,count,ok,failed,skipped}]
    QHash<QString, int> m_uuid_group;      // uuid -> group id
    QHash<int, QString> m_group_color;     // group id -> color string
    int m_selected_group = -1;             // -1 = 全部组
    bool m_auto_switched = false;
    int m_queue_total = 0;
    bool m_queue_has_result = false;
    int m_image_revision = 0;
    int m_error_revision = 0;
    // 每个节点最近一次执行的错误文本（空串表示成功）
    QHash<QString, QString> m_node_errors;
    // 每个节点最近一次提交后的参数与名称快照，用于 diff 出参数/名称变更命令
    QHash<BaseNode*, QPair<QVariantMap, QString>> m_last_state;
    NodeManager(QObject *parent = nullptr) : QObject(parent) {
        m_queue_proxy.setSourceModel(&m_exec_queue);
        // 「全部组」按组连续排列（组内保持执行顺序），便于组间分隔
        m_queue_proxy.setSortRole(ExecQueueModel::GroupRole);
        m_queue_proxy.sort(0, Qt::AscendingOrder);
        connect(&m_executor, &GraphExecutor::statusChanged, this, &NodeManager::engineChanged);
        QObject::connect(&m_executor, &GraphExecutor::nodeImageReady, this,
                         [this](const QString& uuid, const QImage& img) {
            ImageStore::instance()->setThumbnail(uuid, img);
            ++m_image_revision;
            emit imageRevisionChanged();
        });
        QObject::connect(&m_executor, &GraphExecutor::nodeDisplay, this,
                         [this](const QString& uuid, const QVariantMap& d) {
            // m_paint_board 可能为空（尚未挂载画布）或节点未实现 setDisplayData，
            // 此时 invokeMethod 返回 false，安全忽略即可。
            if (!m_paint_board) return;
            for (BaseNode* n : m_paint_board->m_graph.getAllNodes()) {
                if (n->uuid().toString() == uuid) {
                    QMetaObject::invokeMethod(n, "setDisplayData", Qt::DirectConnection,
                                              Q_ARG(QVariantMap, d));
                    break;
                }
            }
        });
        QObject::connect(&m_executor, &GraphExecutor::nodeStarted, this,
                         [this](const QString& uuid) {
            const int grp = groupOf(uuid);
            m_exec_queue.addRunning(uuid, nameOf(uuid), grp, colorOf(grp));
            emit queueChanged();
        });
        // nodeFinished 经队列信号回到 GUI 线程，可安全更新错误表
        QObject::connect(&m_executor, &GraphExecutor::nodeFinished, this,
                         [this](const QString& uuid, int status, const QString& error, int ms) {
            const int grp = groupOf(uuid);
            // 跳过/环等节点从不发 nodeStarted，此时补建终结行
            if (!m_exec_queue.finishNode(uuid, status, error, ms))
                m_exec_queue.addFinished(uuid, nameOf(uuid), status, error, ms, grp, colorOf(grp));
            m_node_errors[uuid] = (status == int(NodeStatus::Ok)) ? QString() : error;
            ++m_error_revision;
            emit errorRevisionChanged();
            const bool bad = (status == int(NodeStatus::Failed) || status == int(NodeStatus::Skipped));
            if (bad && !m_auto_switched && grp >= 0) {
                m_auto_switched = true;
                setSelectedGroup(grp);
                emit focusQueueNode(uuid);
            }
            rebuildGroupSummary();
            emit queueChanged();
        });
        connect(&m_executor, &GraphExecutor::runningChanged, this, [this] {
            if (m_executor.running()) {
                computeGroupsFromGraph();
                m_exec_queue.beginRun();
                m_queue_total = nodeCount();
                m_selected_group = -1;
                m_queue_proxy.setGroup(-1);
                m_auto_switched = false;
                m_queue_has_result = true;
                rebuildGroupSummary();
                emit selectedGroupChanged();
                emit queueChanged();
            }
            emit engineChanged();
        });
    }

    void clearNodeErrors(){
        if(m_node_errors.isEmpty()) return;
        m_node_errors.clear();
        ++m_error_revision;
        emit errorRevisionChanged();
    }
    void resetQueue(){
        m_exec_queue.beginRun();
        m_queue_total = 0;
        m_queue_has_result = false;
        m_selected_group = -1;
        m_queue_proxy.setGroup(-1);
        m_auto_switched = false;
        m_uuid_group.clear();
        m_group_color.clear();
        m_group_summary.clear();
        emit selectedGroupChanged();
        emit queueChanged();
    }
    void refresh(){ if(m_paint_board) m_paint_board->update(); emit graphChanged(); }
    QString nameOf(const QString& uuid) const {
        if (!m_paint_board) return uuid;
        for (BaseNode* n : m_paint_board->m_graph.getAllNodes())
            if (n->uuid().toString() == uuid) return n->name();
        return uuid;
    }
    void computeGroupsFromGraph() {
        m_uuid_group.clear();
        m_group_color.clear();
        m_group_summary.clear();
        if (!m_paint_board) return;
        const QVector<BaseNode*> nodes = m_paint_board->m_graph.getAllNodes();
        QHash<BaseNode*, int> indexOf;
        QStringList names;
        for (int i = 0; i < nodes.size(); ++i) {
            indexOf.insert(nodes.at(i), i);
            names.append(nodes.at(i)->name());
        }
        QVector<QPair<int, int>> edges;
        for (const Edge& e : m_paint_board->m_graph.getAllEdges()) {
            if (!e.start_port || !e.stop_port) continue;
            BaseNode* a = e.start_port->father();
            BaseNode* b = e.stop_port->father();
            if (!a || !b || !indexOf.contains(a) || !indexOf.contains(b)) continue;
            edges.append({ indexOf.value(a), indexOf.value(b) });
        }
        QVector<int> idxToGroup;
        const QVector<QueueGroupInfo> groups = computeQueueGroups(names, edges, idxToGroup);
        QVariantList summary;
        for (const QueueGroupInfo& g : groups) {
            m_group_color.insert(g.id, g.color.name());
            summary.append(QVariantMap{
                { "id", g.id }, { "name", g.name },
                { "color", g.color.name() }, { "count", g.count },
                { "ok", 0 }, { "failed", 0 }, { "skipped", 0 },
            });
        }
        for (int i = 0; i < nodes.size(); ++i) {
            if (i < idxToGroup.size() && idxToGroup.at(i) >= 0)
                m_uuid_group.insert(nodes.at(i)->uuid().toString(), idxToGroup.at(i));
        }
        m_group_summary = summary;
    }

    int groupOf(const QString& uuid) const { return m_uuid_group.value(uuid, -1); }
    QString colorOf(int group) const { return m_group_color.value(group, QString()); }

    void rebuildGroupSummary() {
        for (int i = 0; i < m_group_summary.size(); ++i) {
            QVariantMap g = m_group_summary.at(i).toMap();
            const int id = g.value("id").toInt();
            const int ok = m_exec_queue.countDone(id) - m_exec_queue.countFailed(id)
                           - m_exec_queue.countSkipped(id) - m_exec_queue.countCancelled(id);
            g["ok"] = ok;
            g["failed"] = m_exec_queue.countFailed(id);
            g["skipped"] = m_exec_queue.countSkipped(id);
            m_group_summary[i] = g;
        }
    }
    void raiseNode(BaseNode* node){
        if(!node || !m_paint_board) return;
        qreal maxz = 0;
        for(auto* n : m_paint_board->m_graph.getAllNodes())
            maxz = std::max(maxz, n->z());
        node->setZ(maxz + 1);
    }
    // 撤销/重做恢复图状态后，用当前值刷新提交基线，避免下次误判为变更
    void syncLastState(){
        if(!m_paint_board) return;
        for(BaseNode* n : m_paint_board->m_graph.getAllNodes())
            m_last_state[n] = { n->params(), n->name() };
    }
    void setSelectedNode(BaseNode* n){
        if(m_selected_node == n) return;
        m_selected_node = n; emit selectionChanged();
    }
    Port* portAt(const QPointF& pos, PortType want){
        if(!m_paint_board) return nullptr;
        Port* best = nullptr;
        qreal best_d = 24.0;
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            auto& ports = (want == PortType::Input) ? node->getInPorts() : node->getOutPorts();
            for(Port* p : ports){
                const qreal d = QLineF(p->position(), pos).length();
                if(d < best_d){ best_d = d; best = p; }
            }
        }
        return best;
    }
    static bool compatible(Port* src, Port* dst){
        if(!src || !dst) return false;
        if(src->father() == dst->father()) return false;
        if(!Port::compatible(src->dataType(), dst->dataType())) return false;
        if(dst->isConnected()) return false;
        return true;
    }
    void setLinkTarget(Port* p){
        if(m_link_target == p) return;
        if(m_link_target) m_link_target->setHighlighted(false);
        m_link_target = p;
        if(m_link_target) m_link_target->setHighlighted(true);
    }

public:
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY graphChanged)
    Q_PROPERTY(BaseNode* selectedNode READ selectedNode NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedEdge READ selectedEdge NOTIFY selectionChanged)
    Q_PROPERTY(bool engineRunning READ engineRunning NOTIFY engineChanged)
    Q_PROPERTY(QString engineStatus READ engineStatus NOTIFY engineChanged)
    Q_PROPERTY(int imageRevision READ imageRevision NOTIFY imageRevisionChanged)
    Q_PROPERTY(int errorRevision READ errorRevision NOTIFY errorRevisionChanged)
    Q_PROPERTY(QAbstractItemModel* execQueue READ execQueue CONSTANT)
    Q_PROPERTY(bool queueHasResult READ queueHasResult NOTIFY queueChanged)
    Q_PROPERTY(int queueTotal READ queueTotal NOTIFY queueChanged)
    Q_PROPERTY(int queueDone READ queueDone NOTIFY queueChanged)
    Q_PROPERTY(int queueFailed READ queueFailed NOTIFY queueChanged)
    Q_PROPERTY(int queueSkipped READ queueSkipped NOTIFY queueChanged)
    Q_PROPERTY(int queueCancelled READ queueCancelled NOTIFY queueChanged)
    Q_PROPERTY(QVariantList queueGroups READ queueGroups NOTIFY queueChanged)
    Q_PROPERTY(int selectedGroup READ selectedGroup WRITE setSelectedGroup NOTIFY selectedGroupChanged)

    bool engineRunning() const { return m_executor.running(); }
    QString engineStatus() const { return m_executor.status(); }
    QAbstractItemModel* execQueue() { return &m_queue_proxy; }
    bool queueHasResult() const { return m_queue_has_result; }
    int queueTotal() const {
        if (m_selected_group < 0) return m_queue_total;
        for (const QVariant& v : m_group_summary)
            if (v.toMap().value("id").toInt() == m_selected_group)
                return v.toMap().value("count").toInt();
        return 0;
    }
    int queueDone() const { return m_exec_queue.countDone(m_selected_group); }
    int queueFailed() const { return m_exec_queue.countFailed(m_selected_group); }
    int queueSkipped() const { return m_exec_queue.countSkipped(m_selected_group); }
    int queueCancelled() const { return m_exec_queue.countCancelled(m_selected_group); }
    QVariantList queueGroups() const { return m_group_summary; }
    int selectedGroup() const { return m_selected_group; }
    void setSelectedGroup(int g) {
        if (g >= 0) {
            bool known = false;
            for (const QVariant& v : m_group_summary)
                if (v.toMap().value("id").toInt() == g) { known = true; break; }
            if (!known) g = -1;
        }
        if (m_selected_group == g) return;
        m_selected_group = g;
        m_queue_proxy.setGroup(g);
        rebuildGroupSummary();
        emit selectedGroupChanged();
        emit queueChanged();
    }
    int imageRevision() const { return m_image_revision; }
    int errorRevision() const { return m_error_revision; }
    Q_INVOKABLE QString nodeError(const QString& uuid) const {
        return m_node_errors.value(uuid);
    }
    Q_INVOKABLE bool run() {
        if (engineRunning()) return false;
        Log::info(QStringLiteral("运行图求值：节点 %1 个，边 %2 条")
                      .arg(nodeCount()).arg(edgeCount()));
        clearNodeErrors();
        // 清空各节点上一轮的显示数据（非端口通道），避免残留过期结果
        if (m_paint_board)
            for (BaseNode* n : m_paint_board->m_graph.getAllNodes())
                QMetaObject::invokeMethod(n, "setDisplayData", Qt::DirectConnection,
                                          Q_ARG(QVariantMap, QVariantMap{}));
        // 有意保留 ImageStore 中的旧图像：新结果到达时逐节点覆盖，
        // 失败节点则维持上一次成功结果，避免运行中预览闪烁消失。
        return m_executor.run();
    }
    Q_INVOKABLE void cancelRun() {
        Log::info(QStringLiteral("取消图求值"));
        m_executor.cancel();
    }
    // 选中节点并请求画布把该节点居中
    Q_INVOKABLE void focusNode(const QString& uuid) {
        if (!m_paint_board) return;
        for (Edge& edge : m_paint_board->m_graph.getAllEdges())
            edge.seleected = false;
        BaseNode* target = nullptr;
        for (BaseNode* n : m_paint_board->m_graph.getAllNodes()) {
            const bool hit = (n->uuid().toString() == uuid);
            n->setSelected(hit);
            if (hit) { target = n; raiseNode(n); }
        }
        setSelectedNode(target);
        if (target)
            emit nodeFocusRequested(target->x() + target->width() / 2.0,
                                    target->y() + target->height() / 2.0);
        refresh();
    }
    // 代理模型中该 uuid 的当前行号；未找到返回 -1
    Q_INVOKABLE int queueIndexOf(const QString& uuid) const {
        for (int i = 0; i < m_queue_proxy.rowCount(); ++i) {
            const QModelIndex idx = m_queue_proxy.index(i, 0);
            if (m_queue_proxy.data(idx, ExecQueueModel::UuidRole).toString() == uuid)
                return i;
        }
        return -1;
    }
    Q_INVOKABLE bool hasImage(const QString& uuid) const {
        return ImageStore::instance()->has(uuid);
    }
    Q_INVOKABLE QString imageUrl(const QString& uuid) const {
        return QStringLiteral("image://nodeimage/") + uuid + QStringLiteral("?v=")
               + QString::number(ImageStore::instance()->rev(uuid));
    }
    // 放大查看用：按需从引擎上一轮结果取全分辨率图，并给出带版本号的 URL
    Q_INVOKABLE QImage fullImage(const QString& uuid) {
        return m_executor.fullImage(uuid);
    }
    Q_INVOKABLE bool hasFullImage(const QString& uuid) const {
        return m_executor.hasFullImage(uuid);
    }
    Q_INVOKABLE QString fullImageUrl(const QString& uuid) const {
        return QStringLiteral("image://nodeimagefull/") + uuid + QStringLiteral("?v=")
               + QString::number(m_image_revision);
    }

    int nodeCount() const { return m_paint_board ? int(m_paint_board->m_graph.getAllNodes().size()) : 0; }
    int edgeCount() const { return m_paint_board ? int(m_paint_board->m_graph.getAllEdges().size()) : 0; }
    BaseNode* selectedNode() const { return m_selected_node; }
    QVariantMap selectedEdge() const {
        QVariantMap m;
        if(!m_paint_board) return m;
        for(const Edge& e : m_paint_board->m_graph.getSelectedEdges()){
            if(!e.start_port || !e.stop_port) continue;
            m["from"]     = e.start_port->father()->uuid().toString();
            m["fromPort"] = e.start_port->father()->getOutPorts().indexOf(e.start_port);
            m["to"]       = e.stop_port->father()->uuid().toString();
            m["toPort"]   = e.stop_port->father()->getInPorts().indexOf(e.stop_port);
            break;
        }
        return m;
    }
    Q_INVOKABLE QVariantList nodeSnapshots() {
        if(!m_paint_board) return {};
        return nodeSnapshotsOf(m_paint_board->m_graph);
    }
    Q_INVOKABLE QVariantList edgeSnapshots() {
        if(!m_paint_board) return {};
        return edgeSnapshotsOf(m_paint_board->m_graph);
    }

    // 将当前图序列化为可 JSON 化的映射：节点类型/位置/尺寸/参数 + 端口索引构成的边
    Q_INVOKABLE QVariantMap graphToMap() const {
        QVariantMap doc;
        if(!m_paint_board) return doc;
        doc["version"] = 1;

        QVariantList nodes;
        for(BaseNode* n : m_paint_board->m_graph.getAllNodes()){
            QVariantMap nm;
            nm["uuid"]   = n->uuid().toString();
            nm["type"]   = n->typeName();
            nm["x"]      = n->x();
            nm["y"]      = n->y();
            nm["w"]      = n->width();
            nm["h"]      = n->height();
            nm["name"]   = n->name();
            nm["params"] = n->params();
            nodes.append(nm);
        }
        doc["nodes"] = nodes;

        QVariantList edges;
        for(const Edge& e : m_paint_board->m_graph.getAllEdges()){
            if(!e.start_port || !e.stop_port) continue;
            BaseNode* from = e.start_port->father();
            BaseNode* to   = e.stop_port->father();
            if(!from || !to) continue;
            QVariantMap em;
            em["fromNode"] = from->uuid().toString();
            em["fromPort"] = from->getOutPorts().indexOf(e.start_port);
            em["toNode"]   = to->uuid().toString();
            em["toPort"]   = to->getInPorts().indexOf(e.stop_port);
            edges.append(em);
        }
        doc["edges"] = edges;
        return doc;
    }

    // 当前图的紧凑 JSON 字符串，用于比较是否有未保存的修改
    Q_INVOKABLE QString graphJsonString() const {
        return QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(graphToMap()))
                                     .toJson(QJsonDocument::Compact));
    }

    Q_INVOKABLE bool saveGraph(const QString& path) {
        const QJsonDocument doc(QJsonObject::fromVariantMap(graphToMap()));
        QFile f(path);
        if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate)){
            Log::warn(QStringLiteral("保存图失败：%1（无法写入）").arg(path));
            return false;
        }
        const QByteArray data = doc.toJson(QJsonDocument::Indented);
        const bool ok = (f.write(data) == data.size());
        f.close();
        Log::info(QStringLiteral("保存图%1：%2").arg(ok ? QString() : QStringLiteral("失败"), path));
        return ok;
    }

    Q_INVOKABLE QVariantMap readGraph(const QString& path) {
        QFile f(path);
        if(!f.open(QIODevice::ReadOnly)){
            Log::warn(QStringLiteral("读取图失败：%1（无法打开）").arg(path));
            return {};
        }
        const QByteArray data = f.readAll();
        f.close();
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if(err.error != QJsonParseError::NoError || !doc.isObject()){
            Log::warn(QStringLiteral("读取图失败：%1（JSON 解析错误）").arg(path));
            return {};
        }
        Log::info(QStringLiteral("读取图：%1").arg(path));
        return doc.object().toVariantMap();
    }

    // 依据 uuid + 端口索引恢复连线（读取图文件时使用）
    Q_INVOKABLE bool addEdgeByUuid(const QString& fromNodeUuid, int fromPort,
                                   const QString& toNodeUuid, int toPort) {
        if(!m_paint_board) return false;
        BaseNode* from = nullptr;
        BaseNode* to = nullptr;
        for(BaseNode* n : m_paint_board->m_graph.getAllNodes()){
            if(n->uuid().toString() == fromNodeUuid) from = n;
            if(n->uuid().toString() == toNodeUuid)   to = n;
        }
        if(!from || !to) return false;
        auto& outs = from->getOutPorts();
        auto& ins  = to->getInPorts();
        if(fromPort < 0 || fromPort >= outs.size()) return false;
        if(toPort < 0 || toPort >= ins.size()) return false;
        auto command = std::make_unique<AddEdgeCMD>(outs[fromPort], ins[toPort], m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        refresh();
        return ok;
    }

    Q_INVOKABLE bool createNode(BaseNode *node){
        if(!node || !m_paint_board) return false;
        auto command = std::make_unique<AddNodeCMD>(node, m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        if(ok){
            raiseNode(node);
            m_last_state[node] = { node->params(), node->name() };
            Log::info(QStringLiteral("创建节点：%1 (%2)")
                          .arg(node->typeName(), node->uuid().toString()));
        }
        refresh();
        return ok;
    }
    Q_INVOKABLE bool removeNode(){
        if(!m_paint_board) return false;
        auto nodes = m_paint_board->m_graph.getSelectedNodes();
        if(nodes.isEmpty()) return false;
        bool any = false;
        for(auto* node : nodes){
            auto command = std::make_unique<RemoveNodeCMD>(node, m_paint_board);
            any = m_cmd_manager.executeCommand(std::move(command)) || any;
        }
        if(any) Log::info(QStringLiteral("删除节点：%1 个").arg(nodes.size()));
        refresh();
        return any;
    }
    Q_INVOKABLE bool removeEdge(){
        if(!m_paint_board) return false;
        auto edges = m_paint_board->m_graph.getSelectedEdges();
        if(edges.isEmpty()) return false;
        bool any = false;
        for(const auto& edge : edges){
            auto command = std::make_unique<RemoveEdgeCMD>(edge.start_port, edge.stop_port, m_paint_board);
            any = m_cmd_manager.executeCommand(std::move(command)) || any;
        }
        if(any) Log::info(QStringLiteral("删除边：%1 条").arg(edges.size()));
        refresh();
        return any;
    }
    Q_INVOKABLE bool undo() {
        bool ok = m_cmd_manager.undo();
        Log::info(QStringLiteral("撤销%1").arg(ok ? QString() : QStringLiteral("（无可撤销）")));
        syncLastState();
        refresh();
        return ok;
    }
    Q_INVOKABLE bool redo() {
        bool ok = m_cmd_manager.redo();
        Log::info(QStringLiteral("重做%1").arg(ok ? QString() : QStringLiteral("（无可重做）")));
        syncLastState();
        refresh();
        return ok;
    }

    // 移动/缩放/参数编辑在 QML 中先实时改值，交互结束后在此提交为一条可撤销命令
    Q_INVOKABLE void commitNodeMove(QUuid uid, qreal oldX, qreal oldY){
        if(!m_paint_board) return;
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() != uid) continue;
            const QPointF old_pos(oldX, oldY);
            if(node->position() == old_pos) return;
            auto cmd = std::make_unique<MoveNodeCMD>(node, old_pos, node->position(), m_paint_board);
            if(m_cmd_manager.executeCommand(std::move(cmd)))
                Log::info(QStringLiteral("移动节点：%1").arg(node->typeName()));
            refresh();
            return;
        }
    }
    Q_INVOKABLE void commitNodeResize(QUuid uid, qreal oldW, qreal oldH){
        if(!m_paint_board) return;
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() != uid) continue;
            const QSizeF old_size(oldW, oldH);
            const QSizeF new_size(node->width(), node->height());
            if(old_size == new_size) return;
            auto cmd = std::make_unique<ResizeNodeCMD>(node, old_size, new_size, m_paint_board);
            if(m_cmd_manager.executeCommand(std::move(cmd)))
                Log::info(QStringLiteral("缩放节点：%1").arg(node->typeName()));
            refresh();
            return;
        }
    }
    Q_INVOKABLE void commitNodeParams(QUuid uid){
        if(!m_paint_board) return;
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() != uid) continue;
            // 无基线时先初始化，避免把首次编辑误记成变更
            if(!m_last_state.contains(node))
                m_last_state[node] = { node->params(), node->name() };
            const QVariantMap cur = node->params();
            const QString cur_name = node->name();
            const auto last = m_last_state.value(node);
            if(cur == last.first && cur_name == last.second) return;
            auto cmd = std::make_unique<ChangeParamsCMD>(
                node, last.first, cur, last.second, cur_name, m_paint_board);
            if(m_cmd_manager.executeCommand(std::move(cmd))){
                m_last_state[node] = { cur, cur_name };
                Log::info(QStringLiteral("修改参数：%1").arg(node->typeName()));
            }
            refresh();
            return;
        }
    }

    Q_INVOKABLE void bringToFront(QUuid uid){
        if(!m_paint_board) return;
        for(auto* n : m_paint_board->m_graph.getAllNodes())
            if(n->uuid() == uid) n->setZ(1);
        refresh();
    }
    Q_INVOKABLE void disconnectNode(QUuid uid){
        if(!m_paint_board) return;
        BaseNode* target = nullptr;
        for(auto* n : m_paint_board->m_graph.getAllNodes())
            if(n->uuid() == uid){ target = n; break; }
        if(!target) return;
        const auto incident = m_paint_board->m_graph.edgesOf(target);
        for(const Edge& e : incident){
            auto cmd = std::make_unique<RemoveEdgeCMD>(e.start_port, e.stop_port, m_paint_board);
            m_cmd_manager.executeCommand(std::move(cmd));
        }
        refresh();
    }
    // 该节点当前是否有任意连线（用于 setParams 判断能否安全重建端口）
    Q_INVOKABLE bool nodeHasEdges(BaseNode* node) const {
        if(!node || !m_paint_board) return false;
        for(const Edge& e : m_paint_board->m_graph.getAllEdges()){
            if(!e.start_port || !e.stop_port) continue;
            if(e.start_port->father() == node || e.stop_port->father() == node)
                return true;
        }
        return false;
    }
    // 重建节点端口：先直接删边，再重建端口，避免留下悬垂 Port*
    Q_INVOKABLE void rebuildNodePorts(BaseNode* node, const QVector<PortSpec>& ins,
                                      const QVector<PortSpec>& outs){
        if(!m_paint_board || !node) return;
        // 1) 先删除涉及该节点旧端口的所有边。此处刻意不使用命令入栈：
        //    RemoveEdgeCMD 持有裸 Port*，若入栈则撤销/重做会访问即将被删除的端口。
        //    按值复制边列表，避免删除过程中迭代器失效。
        const QVector<Edge> edges = m_paint_board->m_graph.getAllEdges();
        for(const Edge& e : edges){
            if(!e.start_port || !e.stop_port) continue;
            if(e.start_port->father() == node || e.stop_port->father() == node)
                m_paint_board->m_graph.removeEdge(e.start_port, e.stop_port);
        }
        // 2) 清空历史：旧历史项可能引用本节点刚删除的端口或已删除的边指针，
        //    保留会导致后续撤销访问悬垂指针；不做选择性裁剪，整体清空最安全。
        m_cmd_manager.clear();
        // 3) 旧边已删除，重建端口不再有悬垂引用
        node->rebuildPorts(ins, outs);
        syncLastState();
        refresh();
    }
    Q_INVOKABLE void clearGraph(){
        if(!m_paint_board) return;
        // 清空图前先取消正在运行的求值，避免工作线程在节点已删除后回写图像/错误
        Log::info(QStringLiteral("清空画布：节点 %1 个").arg(m_paint_board->m_graph.getAllNodes().size()));
        m_executor.cancel();
        const auto all = m_paint_board->m_graph.getAllNodes();
        for(auto* n : all){
            auto cmd = std::make_unique<RemoveNodeCMD>(n, m_paint_board);
            m_cmd_manager.executeCommand(std::move(cmd));
        }
        ImageStore::instance()->clear();
        m_executor.clearFullImages();
        ++m_image_revision;
        emit imageRevisionChanged();
        clearNodeErrors();
        resetQueue();
        setSelectedNode(nullptr);
        refresh();
    }

    Q_INVOKABLE void clickNodeEvent(QUuid node_uid, bool ctrl = false) {
        if(!m_paint_board) return;
        ctrl = ctrl && Settings::settings()->ctrlMultiSelect();
        for(Edge& edge : m_paint_board->m_graph.getAllEdges())
            edge.seleected = false;
        BaseNode* hit_node = nullptr;
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            bool hit = (node->uuid() == node_uid);
            if(hit){
                raiseNode(node);
                node->setSelected(ctrl ? !node->selected() : true);
                hit_node = node->selected() ? node : nullptr;
            }else{
                if(!ctrl) node->setSelected(false);
            }
        }
        setSelectedNode(hit_node);
        refresh();
    }
    Q_INVOKABLE void setPaintBoard(PaintBoard *board) {
        m_paint_board = board;
        if(board) m_executor.setGraph(&board->m_graph);
    }

    Q_INVOKABLE void nodeMoveEvent(QUuid node_uid, qreal dx, qreal dy){
        if(!m_paint_board) return;
        QPointF d_pos(dx, dy);
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() == node_uid){
                for(auto* port : node->getPorts())
                    port->movedeltaPos(d_pos);
                break;
            }
        }
        refresh();
    }
    Q_INVOKABLE void nodeResizeEvent(QUuid node_uid, qreal dw, qreal dh){
        if(!m_paint_board) return;
        (void)dh;
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() == node_uid){
                for(auto* port : node->getOutPorts())
                    port->movedeltaPos(QPointF(dw, 0));
                break;
            }
        }
        refresh();
    }
    Q_INVOKABLE void mouseMoveEvent(qreal x, qreal y){
        if(!m_paint_board) return;
        const QPointF pos(x, y);
        m_paint_board->moveDrawing(pos);
        auto* st = Settings::settings();
        if(st->hoverHighlight() || st->midpointMode() == QStringLiteral("hover")){
            const LinkRenderMode mode = Edge::modeFrom(st->renderMode());
            const auto& edges = m_paint_board->m_graph.getAllEdges();
            int hit = -1;
            for(int i = 0; i < edges.size(); ++i){
                if(edges[i].isPointOnCurve(pos, mode)){
                    hit = i;
                    break;
                }
            }
            m_paint_board->setHoveredEdge(hit);
        } else {
            m_paint_board->setHoveredEdge(-1);
        }
    }
    // 世界坐标 pos 处最上层的节点（z 最大；z 相同取遍历顺序靠后 = 子项堆叠靠上）。
    // 供命中测试与 QML 控件做“最上层校验”，避免隔着上层节点操作被压住的下层节点。
    BaseNode* topNodeAtWorld(const QPointF& pos){
        if(!m_paint_board) return nullptr;
        BaseNode* hit = nullptr;
        qreal best = std::numeric_limits<qreal>::lowest();
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            // pos 是图（世界）坐标：节点在 world 内含 Scale/Translate 变换，而 m_paint_board
            // 是未变换的视口项，用它映射会在 zoom/pan 非恒等时整体偏移，故用节点的父层。
            QQuickItem* space = node->parentItem() ? node->parentItem()
                                                   : static_cast<QQuickItem*>(m_paint_board);
            if(node->contains(node->mapFromItem(space, pos)) && node->z() >= best){
                best = node->z();
                hit = node;
            }
        }
        return hit;
    }
    // 世界坐标处最上层节点的 uuid；没有返回空串
    Q_INVOKABLE QString topNodeUuidAt(qreal x, qreal y){
        BaseNode* n = topNodeAtWorld(QPointF(x, y));
        return n ? n->uuid().toString() : QString();
    }
    // 把指定节点提到最上层（不居中、不改选中）
    Q_INVOKABLE void bringToFront(const QString& uuid){
        if(!m_paint_board) return;
        for(BaseNode* n : m_paint_board->m_graph.getAllNodes())
            if(n->uuid().toString() == uuid){ raiseNode(n); refresh(); return; }
    }

    Q_INVOKABLE void mousePressEvent(const QPointF& pos, bool ctrl = false){
        if(!m_paint_board) return;
        ctrl = ctrl && Settings::settings()->ctrlMultiSelect();
        BaseNode* hit_node = topNodeAtWorld(pos);
        const bool node_hit = (hit_node != nullptr);
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            if(node == hit_node)
                node->setSelected(ctrl ? !node->selected() : true);
            else if(!ctrl)
                node->setSelected(false);
        }
        if(hit_node && hit_node->selected() == false)
            hit_node = nullptr;   // ctrl 反选后不再作为选中目标
        const LinkRenderMode mode = Edge::modeFrom(Settings::settings()->renderMode());
        for(Edge& edge : m_paint_board->m_graph.getAllEdges())
            edge.seleected = node_hit ? false : edge.isPointOnCurve(pos, mode);
        setSelectedNode(hit_node);
        refresh();
    }
    Q_INVOKABLE void setOutputPort(Port* port, qreal x, qreal y){
        if(!m_paint_board || !port) return;
        port->setPosition(QPointF(x, y));
        if(!m_paint_board->m_drawing_line){
            m_paint_board->startDrawing(port, QPointF(x, y));
            refresh();
        }
    }
    Q_INVOKABLE void setInputPort(Port* port, qreal x, qreal y){
        if(!m_paint_board || !port) return;
        if(!m_paint_board->m_drawing_line) return;
        auto* src = m_paint_board->m_drawing_edge.start_port;
        if(!src){ m_paint_board->cancelDrawing(); refresh(); return; }
        if(port->isConnected()){
            Log::warn(QStringLiteral("连接失败：目标输入端口已被占用"));
            m_paint_board->cancelDrawing();
            refresh();
            return;
        }
        if(port->father() == src->father()){
            Log::warn(QStringLiteral("连接失败：不能连接同一节点的端口"));
            m_paint_board->cancelDrawing();
            refresh();
            return;
        }
        if(!Port::compatible(port->dataType(), src->dataType())){
            Log::warn(QStringLiteral("连接失败：数据类型不兼容"));
            m_paint_board->cancelDrawing();
            refresh();
            return;
        }
        port->setPosition(QPointF(x, y));
        auto command = std::make_unique<AddEdgeCMD>(src, port, m_paint_board);
        if(m_cmd_manager.executeCommand(std::move(command)))
            Log::info(QStringLiteral("连接：%1 → %2")
                          .arg(src->father()->typeName(), port->father()->typeName()));
        m_paint_board->finishDrawing();
        refresh();
    }

    Q_INVOKABLE void beginLink(Port* port, qreal x, qreal y){
        if(!m_paint_board || !port) return;
        m_paint_board->cancelDrawing();
        setLinkTarget(nullptr);
        m_link_src = port;
        m_link_from_input = (port->type() == static_cast<int>(PortType::Input));
        if(m_link_from_input)
            m_paint_board->startDrawingReverse(port, QPointF(x, y));
        else
            m_paint_board->startDrawing(port, QPointF(x, y));
        refresh();
    }
    Q_INVOKABLE void updateLink(qreal x, qreal y){
        if(!m_paint_board || !m_link_src) return;
        const QPointF pos(x, y);
        m_paint_board->moveDrawing(pos);
        Port* cand = m_link_from_input ? portAt(pos, PortType::Output)
                                       : portAt(pos, PortType::Input);
        Port* src = m_link_from_input ? cand : m_link_src;
        Port* dst = m_link_from_input ? m_link_src : cand;
        setLinkTarget(compatible(src, dst) ? cand : nullptr);
    }
    Q_INVOKABLE void endLink(qreal x, qreal y){
        if(!m_paint_board || !m_link_src) return;
        Port* src = nullptr;
        Port* dst = nullptr;
        // 以释放落点重新判定目标（落点稍偏也能连上）；其次回退到拖拽中的高亮目标
        Port* target = m_link_from_input ? portAt(QPointF(x, y), PortType::Output)
                                         : portAt(QPointF(x, y), PortType::Input);
        if(!target) target = m_link_target;
        if(target){
            Port* s2 = m_link_from_input ? target : m_link_src;
            Port* d2 = m_link_from_input ? m_link_src : target;
            if(Port::compatible(s2->dataType(), d2->dataType())){ src = s2; dst = d2; }
        }
        if(src && dst){
            auto command = std::make_unique<AddEdgeCMD>(src, dst, m_paint_board);
            if(m_cmd_manager.executeCommand(std::move(command)))
                Log::info(QStringLiteral("连接：%1 → %2")
                              .arg(src->father()->typeName(), dst->father()->typeName()));
        } else {
            Log::warn(QStringLiteral("连接失败：未找到兼容的目标端口"));
        }
        m_paint_board->finishDrawing();
        setLinkTarget(nullptr);
        m_link_src = nullptr;
        m_link_from_input = false;
        refresh();
    }
    Q_INVOKABLE void cancelLink(){
        if(m_paint_board) m_paint_board->cancelDrawing();
        setLinkTarget(nullptr);
        m_link_src = nullptr;
        m_link_from_input = false;
        refresh();
    }

signals:
    void graphChanged();
    void selectionChanged();
    void engineChanged();
    void imageRevisionChanged();
    void errorRevisionChanged();
    void queueChanged();
    void selectedGroupChanged();
    void focusQueueNode(const QString& uuid);
    void nodeFocusRequested(qreal wx, qreal wy);

public:
    NodeManager(const NodeManager&) = delete;
    NodeManager& operator=(const NodeManager&) = delete;
    static QObject* instance() {
        static NodeManager instance;
        return &instance;
    }
    ~NodeManager(){};
};
