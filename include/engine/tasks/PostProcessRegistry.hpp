#pragma once
#include <utility>
#include "engine/tasks/TaskSpec.hpp"

// 后处理任务注册表（单例，内联存储，风格同 NodeRegistry）。
// 内置任务在 Task 6 实现；本任务仅提供框架与空注册表。
class PostProcessRegistry {
public:
    static PostProcessRegistry& instance() { static PostProcessRegistry r; return r; }

    // 同 id 重复注册时忽略，保证多次调用 registerBuiltin* 幂等
    void add(TaskSpec s) {
        if (find(s.id)) return;
        m_list.push_back(std::move(s));
    }

    const QVector<TaskSpec>& all() const { return m_list; }

    const TaskSpec* find(const QString& id) const {
        for (const auto& s : m_list)
            if (s.id == id) return &s;
        return nullptr;
    }

private:
    QVector<TaskSpec> m_list;
};
