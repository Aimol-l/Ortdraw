#pragma once
#include <print>
#include <QObject>
#include <QPointer>
#include <QPointF>
#include "PaintBoard.h"
#include "Settings.h"
#include "utils/DAGraph.hpp"
#include "command/AddNode.hpp"
#include "command/AddEdge.hpp"
#include "command/RemoveEdge.hpp"
#include "command/RemoveNode.hpp"
#include "command/CmdManager.hpp"
#include "utils/Snapshot.hpp"


class NodeManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
private:
    CmdManager m_cmd_manager;
    QPointer<PaintBoard> m_paint_board;
    BaseNode* m_selected_node = nullptr;
    NodeManager(QObject *parent = nullptr) : QObject(parent) {}

    void refresh(){ if(m_paint_board) m_paint_board->update(); emit graphChanged(); }
    void setSelectedNode(BaseNode* n){
        if(m_selected_node == n) return;
        m_selected_node = n; emit selectionChanged();
    }

public:
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY graphChanged)
    Q_PROPERTY(BaseNode* selectedNode READ selectedNode NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedEdge READ selectedEdge NOTIFY selectionChanged)

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

    Q_INVOKABLE bool createNode(BaseNode *node){
        if(!node || !m_paint_board) return false;
        auto command = std::make_unique<AddNodeCMD>(node, m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        if(ok) std::println("创建节点成功");
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
        refresh();
        return any;
    }
    Q_INVOKABLE bool undo() { bool ok = m_cmd_manager.undo(); refresh(); return ok; }
    Q_INVOKABLE bool redo() { bool ok = m_cmd_manager.redo(); refresh(); return ok; }

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
    Q_INVOKABLE void clearGraph(){
        if(!m_paint_board) return;
        const auto all = m_paint_board->m_graph.getAllNodes();
        for(auto* n : all){
            auto cmd = std::make_unique<RemoveNodeCMD>(n, m_paint_board);
            m_cmd_manager.executeCommand(std::move(cmd));
        }
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
                node->setZ(1);
                node->setSelected(ctrl ? !node->selected() : true);
                hit_node = node->selected() ? node : nullptr;
            }else{
                node->setZ(0);
                if(!ctrl) node->setSelected(false);
            }
        }
        setSelectedNode(hit_node);
        refresh();
    }
    Q_INVOKABLE void setPaintBoard(PaintBoard *board) { m_paint_board = board; }

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
    Q_INVOKABLE void mousePressEvent(const QPointF& pos, bool ctrl = false){
        if(!m_paint_board) return;
        ctrl = ctrl && Settings::settings()->ctrlMultiSelect();
        bool node_hit = false;
        BaseNode* hit_node = nullptr;
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            QPointF local = node->mapFromItem(m_paint_board, pos);
            if(node->contains(local)){
                node_hit = true;
                node->setSelected(ctrl ? !node->selected() : true);
                hit_node = node->selected() ? node : nullptr;
            }else if(!ctrl){
                node->setSelected(false);
            }
        }
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
            m_paint_board->cancelDrawing();
            std::println("这个节点已经被连接");
            refresh();
            return;
        }
        if(port->father() == src->father()){
            m_paint_board->cancelDrawing();
            std::println("节点不能自己相连");
            refresh();
            return;
        }
        if(port->dataType() != src->dataType()){
            m_paint_board->cancelDrawing();
            std::println("两端数据类型不匹配");
            refresh();
            return;
        }
        port->setPosition(QPointF(x, y));
        auto command = std::make_unique<AddEdgeCMD>(src, port, m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        if(ok) std::println("连接成功");
        else   std::println("有回路或连线无效");
        m_paint_board->finishDrawing();
        refresh();
    }

signals:
    void graphChanged();
    void selectionChanged();

public:
    NodeManager(const NodeManager&) = delete;
    NodeManager& operator=(const NodeManager&) = delete;
    static QObject* instance() {
        static NodeManager instance;
        return &instance;
    }
    ~NodeManager(){};
};
