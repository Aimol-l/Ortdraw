#pragma once

#include <QPointer>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class AddNodeCMD: public Command {
private:
    QPointer<BaseNode> m_node;
    QPointer<PaintBoard> m_paint_board;
public:
    ~AddNodeCMD() = default;
    AddNodeCMD(BaseNode* node, PaintBoard* paint_board)
        :m_node(node), m_paint_board(paint_board) {}

    bool execute() override {
        if(!m_node || !m_paint_board) return false;
        if(!m_paint_board->m_graph.addNode(m_node)) return false;
        m_node->setVisible(true);
        return true;
    }
    void undo() override {
        if(!m_node || !m_paint_board) return;
        m_paint_board->m_graph.removeNode(m_node);
        m_node->setSelected(false);
        m_node->setVisible(false);
    }
};
