#pragma once
#include <QColor>
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>
#include <algorithm>
#include <functional>

// 一个运行组的静态信息（id 从 0 起，按组内最小节点下标升序）
struct QueueGroupInfo {
    int id = 0;
    QString name;   // 组内最小下标“根节点”名 + “…”
    QColor color;
    int count = 0;  // 组内节点数
};

inline const QVector<QColor>& queueGroupPalette() {
    static const QVector<QColor> p = {
        QColor(QStringLiteral("#0969da")), QColor(QStringLiteral("#8250df")),
        QColor(QStringLiteral("#0d9488")), QColor(QStringLiteral("#b15c00")),
        QColor(QStringLiteral("#9854f1")), QColor(QStringLiteral("#587539")),
    };
    return p;
}

// names 下标即节点顺序；edges 为 (fromIndex, toIndex)。
// 返回各组信息，并填充 indexToGroup（长度 = 节点数）。
inline QVector<QueueGroupInfo> computeQueueGroups(const QStringList& names,
                                                  const QVector<QPair<int, int>>& edges,
                                                  QVector<int>& indexToGroup) {
    const int n = names.size();
    QVector<int> parent(n);
    for (int i = 0; i < n; ++i) parent[i] = i;
    std::function<int(int)> find = [&](int x) {
        while (parent[x] != x) { parent[x] = parent[parent[x]]; x = parent[x]; }
        return x;
    };
    auto unite = [&](int a, int b) {
        a = find(a); b = find(b);
        if (a != b) parent[b] = a;
    };
    QVector<int> indeg(n, 0);
    for (const auto& e : edges) {
        if (e.first < 0 || e.first >= n || e.second < 0 || e.second >= n) continue;
        unite(e.first, e.second);
        indeg[e.second] += 1;
    }

    QHash<int, QVector<int>> members;
    for (int i = 0; i < n; ++i) members[find(i)].append(i);

    QVector<int> roots = members.keys();
    std::sort(roots.begin(), roots.end(), [&](int a, int b) {
        return members.value(a).first() < members.value(b).first();
    });

    indexToGroup.assign(n, -1);
    QVector<QueueGroupInfo> groups;
    const auto& palette = queueGroupPalette();
    for (int gi = 0; gi < roots.size(); ++gi) {
        const QVector<int>& mem = members.value(roots.at(gi));
        QueueGroupInfo info;
        info.id = gi;
        info.count = mem.size();
        info.color = palette.at(gi % palette.size());
        int nameIdx = -1;
        for (int m : mem)
            if (indeg[m] == 0 && (nameIdx < 0 || m < nameIdx)) nameIdx = m;
        if (nameIdx < 0) {
            nameIdx = mem.first();
            for (int m : mem) if (m < nameIdx) nameIdx = m;
        }
        info.name = names.at(nameIdx) + QStringLiteral("…");
        groups.append(info);
        for (int m : mem) indexToGroup[m] = gi;
    }
    return groups;
}
