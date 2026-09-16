#pragma once
#include <QHash>
#include <QList>
#include <QUuid>
#include "Edge.hpp"
#include "node/BaseNode.hpp"

class DAGraph{
private:
    QVector<Edge> m_edges;
    QHash<BaseNode*, QList<BaseNode*>> m_adj_list; //邻接表
    bool hasCycle(BaseNode* node, QHash<BaseNode*, bool> &visited, QHash<BaseNode*, bool> &stack) {
        if (!visited[node]) {
            visited[node] = true;
            stack[node] = true;
            for (const auto &neighbor : m_adj_list[node]) {
                if (!visited[neighbor] && hasCycle(neighbor, visited, stack)) {
                    return true;
                } else if (stack[neighbor]) { // 如果在递归栈中，说明存在环
                    return true;
                }
            }
        }
        stack[node] = false; // 递归结束，回溯
        return false;
    }
public:
    DAGraph() = default;
    ~DAGraph() = default;
    // 添加有向边，并检查是否有环
    bool addNode(BaseNode* node){
        if(!node || m_adj_list.contains(node)) return false;
        m_adj_list[node] = QList<BaseNode*>();
        return true;
    }
    bool addEdge(Port* src, Port* dst) {
        if(!src || !dst || !src->father() || !dst->father()) return false;
        if(!m_adj_list.contains(src->father()) || !m_adj_list.contains(dst->father())) return false;
        if(src->father() == dst->father()) return false;
        if(src->dataType() != dst->dataType()) return false;
        if(dst->isConnected()) return false;
        // 防御性检查：常规路径已被 dst->isConnected() 拦截
        for(const auto& edge : m_edges){
            if(edge.start_port == src && edge.stop_port == dst) return false;
        }
        m_adj_list[src->father()].append(dst->father());
        QHash<BaseNode*, bool> visited;
        QHash<BaseNode*, bool> stack;
        for(auto &node : m_adj_list.keys()){
            if(!visited[node] && hasCycle(node, visited, stack)){
                m_adj_list[src->father()].removeOne(dst->father());
                return false;
            }
        }
        m_edges.append(Edge{src, dst});
        src->setConnected(true);
        dst->setConnected(true);
        return true;
    }
    bool removeEdge(Port* src, Port* dst) {
        for(auto it = m_edges.begin(); it != m_edges.end(); ++it){
            if(it->start_port == src && it->stop_port == dst){
                m_edges.erase(it);
                if(auto jt = m_adj_list.find(src->father()); jt != m_adj_list.end())
                    jt.value().removeOne(dst->father());
                bool src_used = false;
                bool dst_used = false;
                for(const auto& edge : m_edges){
                    if(edge.start_port == src) src_used = true;
                    if(edge.stop_port == dst)  dst_used = true;
                }
                src->setConnected(src_used);
                dst->setConnected(dst_used);
                return true;
            }
        }
        return false;
    }
    bool removeNode(BaseNode* node) {
        if(!node || !m_adj_list.contains(node)) return false;
        QVector<Edge> incident = edgesOf(node);
        for(const Edge& edge : incident)
            removeEdge(edge.start_port, edge.stop_port);
        m_adj_list.remove(node);
        for(auto &neighbors : m_adj_list)
            neighbors.removeAll(node);
        return true;
    }
    QVector<Edge> edgesOf(BaseNode* node) const {
        QVector<Edge> result;
        for(const auto& edge : m_edges){
            if(edge.start_port->father() == node || edge.stop_port->father() == node)
                result.append(edge);
        }
        return result;
    }
    QVector<Edge>& getAllEdges() {
        return m_edges;
    }
    QVector<Edge> getSelectedEdges() {
        QVector<Edge> edges;
        for(auto &edge : m_edges){
            if(edge.seleected){
                edges.append(edge);
            }
        }
        return edges;
    }
    QVector<BaseNode*> getAllNodes(){
        return m_adj_list.keys();
    }
    QVector<BaseNode*> getSelectedNodes(){
        QVector<BaseNode*> nodes;
        for(auto &node : m_adj_list.keys()){
            if(node->selected()){
                nodes.append(node);
            }
        }
        return nodes;
    }
};