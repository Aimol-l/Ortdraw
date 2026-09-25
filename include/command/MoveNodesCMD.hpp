#pragma once

#include <QPointer>
#include <QPointF>
#include <QVector>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

// 成组移动多个节点：记录各节点的新位置与整组共享位移。
// 一次整组拖拽 = 一条命令；execute 幂等（绝对位置），
// 撤销/重做按 ±delta 整组还原；端口随位置差同步。
class MoveNodesCMD : public Command {
public:
    struct Item {
        QPointer<BaseNode> node;
        QPointF newPos;
    };
private:
    QVector<Item> m_items;
    QPointF m_delta;
    QPointer<PaintBoard> m_board;
public:
    ~MoveNodesCMD() = default;
    MoveNodesCMD(QVector<Item> items, QPointF delta, PaintBoard* b)
        : m_items(std::move(items)), m_delta(delta), m_board(b) {}

    bool execute() override {
        bool any = false;
        for (const Item& it : m_items)
            any = moveTo(it.node, it.newPos) || any;
        if (m_board) m_board->update();
        return any;
    }
    void undo() override {
        for (const Item& it : m_items)
            moveTo(it.node, it.newPos - m_delta);
        if (m_board) m_board->update();
    }
private:
    static bool moveTo(const QPointer<BaseNode>& n, const QPointF& p) {
        if (!n) return false;
        const QPointF d = p - n->position();
        n->setX(p.x());
        n->setY(p.y());
        for (auto* port : n->getPorts())
            port->movedeltaPos(d);
        return true;
    }
};
