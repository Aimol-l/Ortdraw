# Ortdraw 多 DAG 运行队列（分组 + 下拉筛选） Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 当一次运行包含多条互不相连的链（多个 DAG）时，状态条队列能按组区分：组数 ≥2 出现下拉可筛选查看某条链，全部组视图用同色边条区分，首次失败/跳过时自动切到出错的链。

**Architecture:** 运行开始时由 `NodeManager` 基于图快照用并查集算连通分量（纯函数 `computeQueueGroups`）；`ExecQueueModel` 每行带 `group`/`groupColor`；`NodeManager` 用 `QSortFilterProxyModel` 子类按所选组过滤队列，并暴露 `queueGroups`/`selectedGroup`；`StatusBar` 加下拉与同色边条。

**Tech Stack:** C++23 / Qt6 (Core, Quick, Test) / 现有 DAGraph、GraphExecutor、ExecQueueModel。

**设计文档：** `docs/superpowers/specs/2026-09-21-ortdraw-multi-dag-queue-design.md`

**通用命令：**
- 构建：`cmake --build build -j4`
- 测试：`ctest --test-dir build --output-on-failure`
- QML：`/usr/lib/qt6/bin/qmllint qml/chrome/StatusBar.qml`

---

## 文件结构

- Create: `include/utils/QueueGroups.hpp` — 连通分量分组纯函数 + 调色板。
- Create: `include/QueueFilterProxyModel.hpp` — 按 `group` 过滤的代理模型。
- Modify: `include/ExecQueueModel.hpp` — 新增 `group`/`groupColor` 角色与按组计数。
- Modify: `include/NodeManager.h` — 分组计算、`queueGroups`/`selectedGroup`、代理、按组计数、自动切组与 `focusQueueNode`。
- Modify: `qml/chrome/StatusBar.qml` — 下拉、同色边条、滚动到聚焦芯片。
- Create: `tests/test_queuegroups.cpp`；Modify: `tests/test_execqueue.cpp`、`tests/CMakeLists.txt`。

---

## Task 1: 连通分量分组纯函数

**Files:**
- Create: `include/utils/QueueGroups.hpp`
- Create: `tests/test_queuegroups.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

Create `tests/test_queuegroups.cpp`:

```cpp
#include <QtTest>
#include "utils/QueueGroups.hpp"

class TestQueueGroups : public QObject {
    Q_OBJECT
private slots:
    void twoChains() {
        // 0->1->2   3->4
        const QStringList names = {"加载图片", "灰度化", "高斯模糊", "裁剪", "图片显示"};
        const QVector<QPair<int,int>> edges = {{0,1},{1,2},{3,4}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 2);
        QCOMPARE(gs.at(0).count, 3);
        QCOMPARE(gs.at(0).name, QStringLiteral("加载图片…"));
        QCOMPARE(gs.at(1).count, 2);
        QCOMPARE(gs.at(1).name, QStringLiteral("裁剪…"));
        QCOMPARE(idx, (QVector<int>{0,0,0,1,1}));
        QVERIFY(gs.at(0).color.isValid());
        QVERIFY(gs.at(0).color != gs.at(1).color);
    }

    void singleChain() {
        const QStringList names = {"A", "B"};
        const QVector<QPair<int,int>> edges = {{0,1}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 1);
        QCOMPARE(gs.at(0).name, QStringLiteral("A…"));
        QCOMPARE(idx, (QVector<int>{0,0}));
    }

    void isolatedNodesAreOwnGroups() {
        const QStringList names = {"A", "B", "C"};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, {}, idx);
        QCOMPARE(gs.size(), 3);
        QCOMPARE(idx, (QVector<int>{0,1,2}));
    }

    void cycleHasNoRootFallsBackToMinIndex() {
        // 0->1->2->0（环），另有 3 单独
        const QStringList names = {"甲", "乙", "丙", "丁"};
        const QVector<QPair<int,int>> edges = {{0,1},{1,2},{2,0}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 2);
        QCOMPARE(gs.at(0).name, QStringLiteral("甲…"));   // 无根 → 最小下标节点名
        QCOMPARE(gs.at(1).name, QStringLiteral("丁…"));
    }

    void convergingRootsUseMinIndexRootName() {
        // 0->2, 1->2：一个分量两个根，取最小下标的根名
        const QStringList names = {"根A", "根B", "汇"};
        const QVector<QPair<int,int>> edges = {{0,2},{1,2}};
        QVector<int> idx;
        const auto gs = computeQueueGroups(names, edges, idx);
        QCOMPARE(gs.size(), 1);
        QCOMPARE(gs.at(0).name, QStringLiteral("根A…"));
        QCOMPARE(idx, (QVector<int>{0,0,0}));
    }
};

QTEST_MAIN(TestQueueGroups)
#include "test_queuegroups.moc"
```

- [ ] **Step 2: 加入 TEST_HEADERS**

在 `tests/CMakeLists.txt` 的 `set(TEST_HEADERS ...)` 中加入：

```cmake
    ${PROJECT_SOURCE_DIR}/include/utils/QueueGroups.hpp
```

（`test_queuegroups.cpp` 由现有 `test_*.cpp` GLOB 自动纳入。）

- [ ] **Step 3: 运行确认失败**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: 失败，`utils/QueueGroups.hpp` 不存在。

- [ ] **Step 4: 实现 QueueGroups.hpp**

Create `include/utils/QueueGroups.hpp`:

```cpp
#pragma once
#include <QColor>
#include <QHash>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVector>

// 一个运行组的静态信息（id 从 0 起，按组内最小节点下标升序）
struct QueueGroupInfo {
    int id = 0;
    QString name;   // 组内最小下标“根节点”名 + “…”
    QColor color;
    int count = 0;  // 组内节点数
};

// 组调色板（按 id 循环）
inline const QVector<QColor>& queueGroupPalette() {
    static const QVector<QColor> p = {
        QColor(QStringLiteral("#0969da")), QColor(QStringLiteral("#8250df")),
        QColor(QStringLiteral("#0d9488")), QColor(QStringLiteral("#b15c00")),
        QColor(QStringLiteral("#9854f1")), QColor(QStringLiteral("#587539")),
    };
    return p;
}

// names.size() 为节点数，下标即节点在 getAllNodes() 中的顺序；
// edges 为 (fromIndex, toIndex)。返回各组信息，并填充 indexToGroup（长度 = 节点数）。
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

    // 分量 → 成员
    QHash<int, QVector<int>> members;
    for (int i = 0; i < n; ++i) members[find(i)].append(i);

    // 分量按“最小成员下标”排序，得到稳定 id
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
        // 最小下标且入度 0 的根；无根则取最小下标节点
        int nameIdx = -1;
        for (int m : mem) {
            if (indeg[m] == 0 && (nameIdx < 0 || m < nameIdx)) nameIdx = m;
        }
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
```

> 注意：文件需 `#include <algorithm>` 与 `#include <functional>`。

- [ ] **Step 5: 运行确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 新增 `test_queuegroups` 通过，`100% tests passed out of 17`。

- [ ] **Step 6: 提交**

```bash
git add include/utils/QueueGroups.hpp tests/test_queuegroups.cpp tests/CMakeLists.txt
git commit -m "feat: connected-component grouping for execution queue"
```

---

## Task 2: ExecQueueModel 组角色与按组计数

**Files:**
- Modify: `include/ExecQueueModel.hpp`
- Test: `tests/test_execqueue.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_execqueue.cpp` 的 `private slots:` 内加入：

```cpp
    void groupRolesAndFilteredCounts() {
        ExecQueueModel m;
        m.addRunning("a", "A", 0, "#0969da");
        m.addRunning("b", "B", 0, "#0969da");
        m.addRunning("c", "C", 1, "#8250df");
        m.finishNode("a", int(NodeStatus::Ok), QString(), 1);
        m.finishNode("b", int(NodeStatus::Failed), QStringLiteral("e"), 2);
        m.finishNode("c", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);

        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupRole).toInt(), 0);
        QCOMPARE(m.data(m.index(2), ExecQueueModel::GroupRole).toInt(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::GroupColorRole).toString(), QString("#0969da"));
        QVERIFY(m.roleNames().values().contains(QByteArray("group")));
        QVERIFY(m.roleNames().values().contains(QByteArray("groupColor")));

        QCOMPARE(m.countDone(), 3);
        QCOMPARE(m.countDone(0), 2);
        QCOMPARE(m.countDone(1), 1);
        QCOMPARE(m.countFailed(0), 1);
        QCOMPARE(m.countFailed(1), 0);
        QCOMPARE(m.countSkipped(1), 1);
    }
```

- [ ] **Step 2: 运行确认失败**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: 编译失败（`GroupRole` 不存在）。

- [ ] **Step 3: 实现**

在 `include/ExecQueueModel.hpp`：

1) 角色枚举与 roleNames 增加 `group` / `groupColor`：

```cpp
    enum Roles { UuidRole = Qt::UserRole + 1, NameRole, StatusRole, MsRole, ErrorRole,
                 GroupRole, GroupColorRole };
```

```cpp
        case GroupRole:      return r.group;
        case GroupColorRole: return r.groupColor;
```

```cpp
        return { { UuidRole, "uuid" }, { NameRole, "name" }, { StatusRole, "status" },
                 { MsRole, "ms" }, { ErrorRole, "error" },
                 { GroupRole, "group" }, { GroupColorRole, "groupColor" } };
```

2) 行结构、追加方法带组信息（默认值使旧调用仍可编译）：

```cpp
    struct Row { QString uuid, name, status, error, groupColor; int ms = 0; int group = -1; };
```

```cpp
    void addRunning(const QString& uuid, const QString& name,
                    int group = -1, const QString& groupColor = QString()) {
        const int row = m_rows.size();
        beginInsertRows(QModelIndex(), row, row);
        m_rows.push_back(Row{ uuid, name, runningStatus(), QString(), groupColor, 0, group });
        endInsertRows();
    }
```

```cpp
    void addFinished(const QString& uuid, const QString& name, int status,
                     const QString& error, int ms,
                     int group = -1, const QString& groupColor = QString()) {
        const int row = m_rows.size();
        beginInsertRows(QModelIndex(), row, row);
        m_rows.push_back(Row{ uuid, name, statusText(status), error, groupColor, ms, group });
        endInsertRows();
    }
```

3) 按组计数（`group < 0` 表示全部）：

```cpp
    static bool inGroup(const Row& r, int group) { return group < 0 || r.group == group; }
    int countDone(int group = -1) const {
        int n = 0;
        for (const Row& r : m_rows) if (inGroup(r, group) && r.status != runningStatus()) ++n;
        return n;
    }
    int countFailed(int group = -1) const { return countIn(group, "failed"); }
    int countSkipped(int group = -1) const { return countIn(group, "skipped"); }
    int countCancelled(int group = -1) const { return countIn(group, "cancelled"); }
```

```cpp
    int countIn(int group, const QString& status) const {
        int n = 0;
        for (const Row& r : m_rows) if (inGroup(r, group) && r.status == status) ++n;
        return n;
    }
```

（删除原先的 `countFailed()/...` 无参实现与 `template count`，避免重复定义；`isTerminal` 保留。）

- [ ] **Step 4: 运行确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: `test_execqueue` 通过；`100% tests passed out of 17`（NodeManager 旧调用因默认参数不受影响）。

- [ ] **Step 5: 提交**

```bash
git add include/ExecQueueModel.hpp tests/test_execqueue.cpp
git commit -m "feat: group roles and per-group counts in ExecQueueModel"
```

---

## Task 3: NodeManager 分组计算与按组筛选

**Files:**
- Create: `include/QueueFilterProxyModel.hpp`
- Modify: `include/NodeManager.h`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 代理模型**

Create `include/QueueFilterProxyModel.hpp`:

```cpp
#pragma once
#include <QSortFilterProxyModel>

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
```

> 需要包含 `ExecQueueModel.hpp`（为 `GroupRole`）；在该头文件顶部加 `#include "ExecQueueModel.hpp"`。

在 `tests/CMakeLists.txt` 的 `TEST_HEADERS` 加入：

```cmake
    ${PROJECT_SOURCE_DIR}/include/QueueFilterProxyModel.hpp
```

- [ ] **Step 2: NodeManager 成员与 include**

在 `include/NodeManager.h` include 区加入：

```cpp
#include "QueueFilterProxyModel.hpp"
#include "utils/QueueGroups.hpp"
```

在成员区加入：

```cpp
    QueueFilterProxyModel m_queue_proxy;
    QVariantList m_group_summary;          // [{id,name,color,count}]（count 为运行快照组内节点数）
    QHash<QString, int> m_uuid_group;      // uuid -> group id
    QHash<int, QString> m_group_color;     // group id -> color string
    int m_selected_group = -1;             // -1 = 全部组
    bool m_auto_switched = false;
```

在构造函数里（`m_exec_queue` 建好后）设置代理源：

```cpp
        m_queue_proxy.setSourceModel(&m_exec_queue);
```

- [ ] **Step 3: 运行开始时算组**

把 `runningChanged` 连接中的运行分支改为：

```cpp
        connect(&m_executor, &GraphExecutor::runningChanged, this, [this] {
            if (m_executor.running()) {
                computeGroupsFromGraph();
                m_exec_queue.beginRun();
                m_selected_group = -1;
                m_queue_proxy.setGroup(-1);
                m_auto_switched = false;
                m_queue_has_result = true;
                rebuildGroupSummary();
                emit selectedGroupChanged();
                emit queueChanged();
            }
            emit engineChanged();
        });
```

新增私有函数（放在 `nameOf` 附近）：

```cpp
    void computeGroupsFromGraph() {
        m_uuid_group.clear();
        m_group_color.clear();
        m_group_summary.clear();
        if (!m_paint_board) return;
        const QVector<BaseNode*> nodes = m_paint_board->m_graph.getAllNodes();
        QHash<BaseNode*, int> indexOf;
        QStringList names;
        for (int i = 0; i < nodes.size(); ++i) {
            indexOf.insert(nodes.at(i), i);
            names.append(nodes.at(i)->name());
        }
        QVector<QPair<int, int>> edges;
        for (const Edge& e : m_paint_board->m_graph.getAllEdges()) {
            if (!e.start_port || !e.stop_port) continue;
            BaseNode* a = e.start_port->father();
            BaseNode* b = e.stop_port->father();
            if (!a || !b || !indexOf.contains(a) || !indexOf.contains(b)) continue;
            edges.append({ indexOf.value(a), indexOf.value(b) });
        }
        QVector<int> idxToGroup;
        const QVector<QueueGroupInfo> groups = computeQueueGroups(names, edges, idxToGroup);
        QVariantList summary;
        for (const QueueGroupInfo& g : groups) {
            m_group_color.insert(g.id, g.color.name());
            summary.append(QVariantMap{
                { "id", g.id }, { "name", g.name },
                { "color", g.color.name() }, { "count", g.count },
                { "ok", 0 }, { "failed", 0 }, { "skipped", 0 },
            });
        }
        for (int i = 0; i < nodes.size(); ++i) {
            if (i < idxToGroup.size() && idxToGroup.at(i) >= 0)
                m_uuid_group.insert(nodes.at(i)->uuid().toString(), idxToGroup.at(i));
        }
        m_group_summary = summary;
    }

    int groupOf(const QString& uuid) const { return m_uuid_group.value(uuid, -1); }
    QString colorOf(int group) const { return m_group_color.value(group, QString()); }

    void rebuildGroupSummary() {
        for (int i = 0; i < m_group_summary.size(); ++i) {
            QVariantMap g = m_group_summary.at(i).toMap();
            const int id = g.value("id").toInt();
            const int ok = m_exec_queue.countDone(id) - m_exec_queue.countFailed(id)
                           - m_exec_queue.countSkipped(id) - m_exec_queue.countCancelled(id);
            g["ok"] = ok;
            g["failed"] = m_exec_queue.countFailed(id);
            g["skipped"] = m_exec_queue.countSkipped(id);
            m_group_summary[i] = g;
        }
    }
```

- [ ] **Step 4: 事件带上组信息 + 自动切组**

`nodeStarted`：

```cpp
            m_exec_queue.addRunning(uuid, nameOf(uuid), groupOf(uuid), colorOf(groupOf(uuid)));
            emit queueChanged();
```

`nodeFinished`：

```cpp
        QObject::connect(&m_executor, &GraphExecutor::nodeFinished, this,
                         [this](const QString& uuid, int status, const QString& error, int ms) {
            const int grp = groupOf(uuid);
            if (!m_exec_queue.finishNode(uuid, status, error, ms))
                m_exec_queue.addFinished(uuid, nameOf(uuid), status, error, ms, grp, colorOf(grp));
            m_node_errors[uuid] = (status == int(NodeStatus::Ok)) ? QString() : error;
            ++m_error_revision;
            emit errorRevisionChanged();
            const bool bad = (status == int(NodeStatus::Failed) || status == int(NodeStatus::Skipped));
            if (bad && !m_auto_switched && grp >= 0) {
                m_auto_switched = true;
                setSelectedGroup(grp);
                emit focusQueueNode(uuid);
            }
            rebuildGroupSummary();
            emit queueChanged();
        });
```

- [ ] **Step 5: 属性与访问器**

在 `Q_PROPERTY` 区加入：

```cpp
    Q_PROPERTY(QVariantList queueGroups READ queueGroups NOTIFY queueChanged)
    Q_PROPERTY(int selectedGroup READ selectedGroup WRITE setSelectedGroup NOTIFY selectedGroupChanged)
```

`execQueue` 属性改为返回代理（保持 `CONSTANT`）：

```cpp
    QAbstractListModel* execQueue() { return &m_queue_proxy; }
```

计数 getter 改为按所选组：

```cpp
    int queueTotal() const {
        if (m_selected_group < 0) return m_queue_total;
        for (const QVariant& v : m_group_summary)
            if (v.toMap().value("id").toInt() == m_selected_group)
                return v.toMap().value("count").toInt();
        return 0;
    }
    int queueDone() const { return m_exec_queue.countDone(m_selected_group); }
    int queueFailed() const { return m_exec_queue.countFailed(m_selected_group); }
    int queueSkipped() const { return m_exec_queue.countSkipped(m_selected_group); }
    int queueCancelled() const { return m_exec_queue.countCancelled(m_selected_group); }
    QVariantList queueGroups() const { return m_group_summary; }
    int selectedGroup() const { return m_selected_group; }
    void setSelectedGroup(int g) {
        if (m_selected_group == g) return;
        m_selected_group = g;
        m_queue_proxy.setGroup(g);
        rebuildGroupSummary();
        emit selectedGroupChanged();
        emit queueChanged();
    }
```

> 注意：`m_exec_queue.countDone(...)` 是 `const` 方法，但 `m_exec_queue` 是成员；这些 getter 需为 `const`（Q_PROPERTY READ 用）。`m_queue_proxy.setGroup` 非 const，只在 `setSelectedGroup` 调用，不在 const getter 内，故无冲突。

- [ ] **Step 6: 复位与信号**

`resetQueue()` 增加：

```cpp
    void resetQueue(){
        m_exec_queue.beginRun();
        m_queue_total = 0;
        m_queue_has_result = false;
        m_selected_group = -1;
        m_queue_proxy.setGroup(-1);
        m_auto_switched = false;
        m_uuid_group.clear();
        m_group_color.clear();
        m_group_summary.clear();
        emit selectedGroupChanged();
        emit queueChanged();
    }
```

信号区加入：

```cpp
    void selectedGroupChanged();
    void focusQueueNode(const QString& uuid);
```

- [ ] **Step 7: 构建 + 测试**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 构建通过；`100% tests passed out of 17`。

- [ ] **Step 8: 提交**

```bash
git add include/QueueFilterProxyModel.hpp include/NodeManager.h tests/CMakeLists.txt
git commit -m "feat: group-aware queue filtering and auto-switch in NodeManager"
```

---

## Task 4: StatusBar 下拉、同色边条与滚动聚焦

**Files:**
- Modify: `qml/chrome/StatusBar.qml`

- [ ] **Step 1: 下拉（仅 ≥2 组时显示）**

在 `qml/chrome/StatusBar.qml` 的 `leftGroup` 之后、`statsGroup` 之前插入：

```qml
    // 组数 ≥ 2 时出现：选择要查看的链
    Rectangle {
        id: groupSel
        visible: root.queueMode && NodeManager.queueGroups.length >= 2
        anchors.left: leftGroup.right
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: Math.round(root.height * 0.62)
        width: selRow.implicitWidth + 16
        radius: Math.round(root.height * 0.15)
        color: selHover.hovered || groupMenu.opened ? Theme.bgHover : Theme.bg
        border.width: 1
        border.color: Theme.border

        function currentLabel() {
            if (NodeManager.selectedGroup < 0) return "全部组 (" + NodeManager.queueGroups.length + ")"
            var gs = NodeManager.queueGroups
            for (var i = 0; i < gs.length; ++i)
                if (gs[i].id === NodeManager.selectedGroup) return gs[i].name
            return "全部组 (" + gs.length + ")"
        }
        function currentColor() {
            if (NodeManager.selectedGroup < 0) return Theme.fgDim
            for (var i = 0; i < NodeManager.queueGroups.length; ++i)
                if (NodeManager.queueGroups[i].id === NodeManager.selectedGroup)
                    return NodeManager.queueGroups[i].color
            return Theme.fgDim
        }

        Row {
            id: selRow
            anchors.centerIn: parent
            spacing: 6
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.round(root.height * 0.2); height: width; radius: width / 2
                color: groupSel.currentColor()
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: groupSel.currentLabel()
                color: Theme.fg
                font.pixelSize: root.fSmall
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "▾"; color: Theme.fgDim; font.pixelSize: root.fSmall
                renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
            }
        }

        HoverHandler { id: selHover }
        TapHandler { onTapped: groupMenu.opened ? groupMenu.close() : groupMenu.open() }

        Menu {
            id: groupMenu
            y: -(height + 6)
            Instantiator {
                model: NodeManager.queueGroups
                delegate: MenuItem {
                    required property var modelData
                    text: {
                        var s = (modelData.id === NodeManager.selectedGroup ? "● " : "") + modelData.name
                        var parts = []
                        if (modelData.failed > 0) parts.push("失败" + modelData.failed)
                        if (modelData.skipped > 0) parts.push("跳过" + modelData.skipped)
                        s += "  · " + modelData.count + " 节点"
                        if (parts.length) s += " · " + parts.join(" ")
                        return s
                    }
                    onTriggered: NodeManager.selectedGroup = modelData.id
                }
                onObjectAdded: (index, object) => groupMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => groupMenu.removeItem(index)
            }
            MenuSeparator {}
            MenuItem {
                text: "全部组"
                onTriggered: NodeManager.selectedGroup = -1
            }
        }
    }
```

- [ ] **Step 2: 队列左边界让位于下拉**

把 `ListView { id: queue ... }` 的：

```qml
        anchors.left: leftGroup.right
        anchors.leftMargin: 16
```

改为：

```qml
        anchors.left: groupSel.visible ? groupSel.right : leftGroup.right
        anchors.leftMargin: 16
```

并把 `statsGroup` 的 `anchors.left: leftGroup.right` 保持（未运行时下拉不可见，无重叠）。

- [ ] **Step 3: 芯片左侧同组色条（仅「全部组」）**

在芯片 `Rectangle { id: chip ... }` 内、`Row { id: chipRow ... }` 之前加入：

```qml
                    // 「全部组」时：同组同色左边条
                    Rectangle {
                        visible: NodeManager.selectedGroup < 0 && del.group >= 0
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 3
                        color: del.groupColor
                    }
```

（delegate 的 `required property` 增加 `group`/`groupColor`，见 Step 4。）

- [ ] **Step 4: delegate 读取组角色**

在 delegate 的 `required property` 列表加入：

```qml
            required property int group
            required property string groupColor
```

- [ ] **Step 5: 滚动到聚焦芯片**

在 delegate 内部（`ToolTip { ... }` 之后、delegate 结束前）加入：

```qml
            Connections {
                target: NodeManager
                function onFocusQueueNode(uuid) {
                    if (del.uuid === uuid)
                        queue.positionViewAtIndex(del.index, ListView.Contain)
                }
            }
```

说明：delegate 内部响应信号，只对已创建的 delegate 生效；正在运行的失败节点通常已在可视范围附近，自动跟随 `scrollToEnd` 亦会覆盖多数情况。若实机发现失败芯片仍在可视区外，在 Task 5 改为在 StatusBar 层按 `queue.model` 的 uuid 角色查找索引后 `positionViewAtIndex`。

- [ ] **Step 6: lint + 构建 + 测试**

Run: `/usr/lib/qt6/bin/qmllint qml/chrome/StatusBar.qml && cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: qmllint 退出 0；测试 `100% tests passed out of 17`。

- [ ] **Step 7: 提交**

```bash
git add qml/chrome/StatusBar.qml
git commit -m "feat(ui): DAG group selector, per-group color bars and scroll-to-focus"
```

---

## Task 5: 实机验证

**Files:** 无

- [ ] **Step 1: 两条链运行**

用隔离配置启动，加载含两条独立链的图（或手动建两条），运行，确认：
- 组数 ≥2 时出现下拉 `全部组 (2) ▾`；
- 「全部组」下芯片带同组色条（蓝/紫）；
- 下拉切到「② 裁剪…」后只显示该链芯片，左侧汇总随组变化；
- 单链的图不出现下拉。

- [ ] **Step 2: 失败自动切组**

让某条链报错（如 `ImageLoad` 路径不存在），运行后确认：
- 首次失败/跳过时自动切到该链、下拉标签变为该组；
- 该失败芯片被滚入可视（`focusQueueNode`）。

- [ ] **Step 3: 日志与清理**

```bash
grep -iE "error|warning" /tmp/ortdraw.log | head
rm -f test_*.ortdraw
```

- [ ] **Step 4: 提交（如有微调）**

```bash
git add -A && git commit -m "chore: polish multi-DAG queue after live verification"
```

---

## Self-Review 记录

- **Spec 覆盖**：§3.1 分组（Task 1）；§3.2 组角色（Task 2）；§3.3 接口/代理/计数（Task 3）；§3.4 自动切组与 `focusQueueNode`（Task 3/4）；§4 UI 下拉/色条/汇总（Task 4）；§5 边界（Task 3 `resetQueue`、Task 1 孤点/环测试）；§7 测试（Task 1/2 + Task 5 实机）。
- **占位符**：无 TBD；每步给出可执行代码或确切命令。
- **类型一致性**：`QueueGroupInfo{id,name,color,count}`（Task 1）= `queueGroups` 元素字段（Task 3/4）；`GroupRole`/`GroupColorRole`（Task 2）在 Task 3 写入、Task 4 读取 `del.group`/`del.groupColor`；`countDone/.../ (int group=-1)`（Task 2）在 Task 3 getter 复用；`selectedGroup`/`focusQueueNode`（Task 3）在 Task 4 使用。
- **已知简化**：滚动聚焦采用 delegate 内响应 `focusQueueNode` + `positionViewAtIndex`（对已创建 delegate 生效）；未创建时依赖自动跟随。若实机发现不足，在 Task 5 改为在 StatusBar 层按模型 uuid 角色查找索引。
