#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Command.h"

class CmdManager {
private:
    std::ptrdiff_t m_cmd_idx = -1;
    std::vector<std::unique_ptr<Command>> m_commands;
public:
    CmdManager() = default;
    ~CmdManager() = default;

    bool executeCommand(std::unique_ptr<Command> command) {
        if(!command) return false;
        if(!command->execute()) return false;
        if(m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size()))
            m_commands.erase(m_commands.begin() + m_cmd_idx + 1, m_commands.end());
        m_commands.push_back(std::move(command));
        ++m_cmd_idx;
        return true;
    }
    bool undo() {
        if(m_cmd_idx >= 0){
            m_commands.at(m_cmd_idx)->undo();
            --m_cmd_idx;
            return true;
        }
        return false;
    }
    bool redo() {
        if(m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size())){
            if(!m_commands.at(m_cmd_idx + 1)->execute()) return false;
            ++m_cmd_idx;
            return true;
        }
        return false;
    }
    void clear() { m_commands.clear(); m_cmd_idx = -1; }
    bool canUndo() const { return m_cmd_idx >= 0; }
    bool canRedo() const {
        return m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size());
    }
};
