#pragma once
#include <QSortFilterProxyModel>
#include "ExecQueueModel.hpp"

// 按 ExecQueueModel 的 GroupRole 过滤；group < 0 表示不过滤。
class QueueFilterProxyModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit QueueFilterProxyModel(QObject* parent = nullptr) : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
    }
    int group() const { return m_group; }
    void setGroup(int g) {
        if (m_group == g) return;
        m_group = g;
        invalidateFilter();
    }
protected:
    bool filterAcceptsRow(int row, const QModelIndex& parent) const override {
        if (m_group < 0) return true;
        const QModelIndex idx = sourceModel()->index(row, 0, parent);
        return sourceModel()->data(idx, ExecQueueModel::GroupRole).toInt() == m_group;
    }
private:
    int m_group = -1;
};
