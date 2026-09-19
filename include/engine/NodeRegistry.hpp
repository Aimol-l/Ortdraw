#pragma once
#include <QHash>
#include <QString>
#include <QStringList>
#include <memory>
#include "engine/NodeExecutor.hpp"

class NodeRegistry {
public:
    static NodeRegistry& instance() { static NodeRegistry r; return r; }
    void registerExecutor(const QString& type, std::shared_ptr<NodeExecutor> exec) {
        m_map.insert(type, std::move(exec));
    }
    std::shared_ptr<NodeExecutor> executorFor(const QString& type) const {
        return m_map.value(type, nullptr);
    }
    QStringList knownTypes() const { return m_map.keys(); }
private:
    QHash<QString, std::shared_ptr<NodeExecutor>> m_map;
};
