#pragma once
#include <QAbstractListModel>
#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>

#include "engine/NodeStatus.hpp"

// 运行队列：按执行顺序逐行追加，状态为 "running"/"ok"/"failed"/"skipped"/"cancelled"。
class ExecQueueModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { UuidRole = Qt::UserRole + 1, NameRole, StatusRole, MsRole, ErrorRole };
    Q_ENUM(Roles)

    explicit ExecQueueModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return parent.isValid() ? 0 : m_rows.size();
    }

    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return {};
        const Row& r = m_rows.at(index.row());
        switch (role) {
        case UuidRole:   return r.uuid;
        case NameRole:   return r.name;
        case StatusRole: return r.status;
        case MsRole:     return r.ms;
        case ErrorRole:  return r.error;
        default:         return {};
        }
    }

    QHash<int, QByteArray> roleNames() const override {
        return { { UuidRole, "uuid" }, { NameRole, "name" }, { StatusRole, "status" },
                 { MsRole, "ms" }, { ErrorRole, "error" } };
    }

    void beginRun() {
        if (m_rows.isEmpty()) return;
        beginResetModel();
        m_rows.clear();
        endResetModel();
    }

    void addRunning(const QString& uuid, const QString& name) {
        const int row = m_rows.size();
        beginInsertRows(QModelIndex(), row, row);
        m_rows.push_back(Row{ uuid, name, QStringLiteral("running"), QString(), 0 });
        endInsertRows();
    }

    // 更新该 uuid 最后一行；返回是否命中（未知 uuid 安全忽略并返回 false）。
    bool finishNode(const QString& uuid, int status, const QString& error, int ms) {
        for (int i = m_rows.size() - 1; i >= 0; --i) {
            if (m_rows.at(i).uuid != uuid) continue;
            m_rows[i].status = statusText(status);
            m_rows[i].error = error;
            m_rows[i].ms = ms;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, { StatusRole, ErrorRole, MsRole });
            return true;
        }
        return false;
    }

    // 追加一条已终结的行：用于“跳过/环”等从未发过 nodeStarted 的节点。
    void addFinished(const QString& uuid, const QString& name, int status,
                     const QString& error, int ms) {
        const int row = m_rows.size();
        beginInsertRows(QModelIndex(), row, row);
        m_rows.push_back(Row{ uuid, name, statusText(status), error, ms });
        endInsertRows();
    }

    static QString statusText(int status) {
        switch (NodeStatus(status)) {
        case NodeStatus::Ok:        return QStringLiteral("ok");
        case NodeStatus::Failed:    return QStringLiteral("failed");
        case NodeStatus::Skipped:   return QStringLiteral("skipped");
        case NodeStatus::Cancelled: return QStringLiteral("cancelled");
        }
        return QStringLiteral("failed");
    }

    bool isTerminal(int row) const {
        if (row < 0 || row >= m_rows.size()) return false;
        return m_rows.at(row).status != QStringLiteral("running");
    }
    int countDone() const { return count([](const Row& r){ return r.status != "running"; }); }
    int countFailed() const { return count([](const Row& r){ return r.status == "failed"; }); }
    int countSkipped() const { return count([](const Row& r){ return r.status == "skipped"; }); }
    int countCancelled() const { return count([](const Row& r){ return r.status == "cancelled"; }); }

private:
    struct Row { QString uuid, name, status, error; int ms = 0; };

    template <typename Pred>
    int count(Pred pred) const {
        int n = 0;
        for (const Row& r : m_rows) if (pred(r)) ++n;
        return n;
    }

    QVector<Row> m_rows;
};
