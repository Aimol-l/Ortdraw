#pragma once

#include <QPointer>
#include <QSizeF>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

// 缩放节点：记录前后尺寸；宽度变化需同步移动输出端口（输出端口靠右）
class ResizeNodeCMD : public Command {
private:
    QPointer<BaseNode> m_node;
    QSizeF m_old;
    QSizeF m_new;
    QPointer<PaintBoard> m_board;
public:
    ~ResizeNodeCMD() = default;
    ResizeNodeCMD(BaseNode* n, QSizeF oldSize, QSizeF newSize, PaintBoard* b)
        : m_node(n), m_old(oldSize), m_new(newSize), m_board(b) {}

    bool execute() override { return resizeTo(m_new); }
    void undo() override { resizeTo(m_old); }
    bool resizeTo(QSizeF s) {
        if(!m_node || !m_board) return false;
        const qreal dw = s.width() - m_node->width();
        m_node->setWidth(s.width());
        m_node->setHeight(s.height());
        for(auto* port : m_node->getOutPorts())
            port->movedeltaPos(QPointF(dw, 0));
        m_board->update();
        return true;
    }
};
