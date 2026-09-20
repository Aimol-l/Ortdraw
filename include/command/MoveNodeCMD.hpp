#pragma once

#include <QPointer>
#include <QPointF>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

// 移动节点：绝对坐标记录前后位置，撤销/重做时按差值同步端口位置
class MoveNodeCMD : public Command {
private:
    QPointer<BaseNode> m_node;
    QPointF m_old;
    QPointF m_new;
    QPointer<PaintBoard> m_board;
public:
    ~MoveNodeCMD() = default;
    MoveNodeCMD(BaseNode* n, QPointF oldPos, QPointF newPos, PaintBoard* b)
        : m_node(n), m_old(oldPos), m_new(newPos), m_board(b) {}

    bool execute() override { return moveTo(m_new); }
    void undo() override { moveTo(m_old); }
    bool moveTo(QPointF p) {
        if(!m_node || !m_board) return false;
        QPointF d = p - m_node->position();
        m_node->setX(p.x());
        m_node->setY(p.y());
        for(auto* port : m_node->getPorts())
            port->movedeltaPos(d);
        m_board->update();
        return true;
    }
};
