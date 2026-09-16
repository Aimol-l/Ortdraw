#pragma once

#include <QPointer>
#include "Command.h"
#include "port/Port.hpp"
#include "PaintBoard.h"

class AddEdgeCMD: public Command {
private:
    Port* m_port_src; // 不持有所有权，不要删除ta
    Port* m_port_dst;
    QPointer<PaintBoard> m_paint_board;
public:
    ~AddEdgeCMD() = default;
    AddEdgeCMD(Port* src, Port* dst, PaintBoard* paint_board)
        :m_port_src(src), m_port_dst(dst),m_paint_board(paint_board) {}

    bool execute() override {
        if(!m_paint_board) return false;
        return m_paint_board->m_graph.addEdge(m_port_src,m_port_dst);
    }
    void undo() override {
        if(!m_paint_board) return;
        m_paint_board->m_graph.removeEdge(m_port_src,m_port_dst);
    }
};
