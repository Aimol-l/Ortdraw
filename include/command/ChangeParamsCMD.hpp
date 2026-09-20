#pragma once

#include <QPointer>
#include <QVariantMap>
#include <QString>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

// 参数/名称变更：参数与名称均为绝对赋值，天然幂等，撤销/重做直接整份写回
class ChangeParamsCMD : public Command {
private:
    QPointer<BaseNode> m_node;
    QPointer<PaintBoard> m_board;
    QVariantMap m_old;
    QVariantMap m_new;
    QString m_old_name;
    QString m_new_name;
public:
    ~ChangeParamsCMD() = default;
    ChangeParamsCMD(BaseNode* n, QVariantMap oldP, QVariantMap newP,
                    QString oldN, QString newN, PaintBoard* b)
        : m_node(n), m_board(b), m_old(oldP), m_new(newP),
          m_old_name(oldN), m_new_name(newN) {}

    bool execute() override { return apply(m_new, m_new_name); }
    void undo() override { apply(m_old, m_old_name); }
    bool apply(const QVariantMap& p, const QString& nm) {
        if(!m_node || !m_board) return false;
        m_node->setParams(p);
        m_node->setName(nm);
        m_board->update();
        return true;
    }
};
