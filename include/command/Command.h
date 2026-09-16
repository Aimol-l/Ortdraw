#pragma once


class Command {
public:
    virtual ~Command() = default;
    virtual void undo() = 0;     // 撤销操作
    virtual bool execute() = 0;  // 执行操作
};
    