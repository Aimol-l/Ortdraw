#pragma once

#include <QPointer>
#include <QVector>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class RemoveNodeCMD : public Command {
private:
    QPointer<BaseNode> m_node;
    QPointer<PaintBoard> m_paint_board;
    // 方案 A 前提：节点对象不被销毁，因此快照中的对端端口在对端节点存活期间有效。
    QVector<Edge> m_snapshot;
public:
    ~RemoveNodeCMD() = default;
    RemoveNodeCMD(BaseNode* node, PaintBoard* paint_board)
        :m_node(node), m_paint_board(paint_board) {}

    bool execute() override {
        if(!m_node || !m_paint_board) return false;
        m_snapshot = m_paint_board->m_graph.edgesOf(m_node);
        if(!m_paint_board->m_graph.removeNode(m_node)) return false;
        m_node->setSelected(false);
        m_node->setVisible(false);
        return true;
    }
    void undo() override {
        if(!m_node || !m_paint_board) return;
        m_paint_board->m_graph.addNode(m_node);
        m_node->setVisible(true);
        for(const Edge& edge : m_snapshot)
            m_paint_board->m_graph.addEdge(edge.start_port, edge.stop_port);
    }
};
