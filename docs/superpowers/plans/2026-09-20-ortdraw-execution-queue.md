# Ortdraw 运行队列（状态条改造） Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把底部状态条中间区域改造成按执行顺序展示节点运行结果的队列（状态 + 单节点耗时、滑入动效、自动跟随、点击定位、失败/跳过悬停提示）。

**Architecture:** 执行引擎新增 `nodeStarted` 与带状态枚举/耗时的 `nodeFinished`；`NodeManager` 用一个 `ExecQueueModel`（`QAbstractListModel`）承接信号并暴露给 QML；`StatusBar` 用横向 `ListView` 渲染队列与动效，动效由 `Settings.queueAnimation` 门控。

**Tech Stack:** C++23 / Qt6 (Quick, Test) / OpenCV / 现有 DAGraph 与 GraphExecutor。

**设计文档：** `docs/superpowers/specs/2026-09-20-ortdraw-execution-queue-design.md`

**构建/测试命令（全篇通用）：**
- 构建：`cmake --build build -j4`
- 测试：`ctest --test-dir build --output-on-failure`
- QML 检查：`/usr/lib/qt6/bin/qmllint qml/chrome/StatusBar.qml`

---

## 文件结构

- Create: `include/engine/NodeStatus.hpp` — 节点状态枚举（各模块共享）。
- Modify: `include/engine/GraphExecutor.hpp` — 信号、状态、耗时、跳过/取消语义。
- Create: `include/ExecQueueModel.hpp` — 运行队列列表模型。
- Modify: `include/NodeManager.h` — 持有模型、接线、计数摘要、`focusNode`。
- Modify: `include/Settings.h`、`qml/settings/SettingsDialog.qml` — `queueAnimation`。
- Modify: `qml/chrome/StatusBar.qml` — 队列 UI/动效/交互。
- Modify: `qml/main.qml` — `nodeFocusRequested` → 画布居中。
- Create: `tests/test_execqueue.cpp`；Modify: `tests/test_graphexecutor.cpp`、`tests/test_settings.cpp`、`tests/CMakeLists.txt`。

---

## Task 1: 执行引擎的状态枚举、开始信号、耗时与跳过/取消

**Files:**
- Create: `include/engine/NodeStatus.hpp`
- Modify: `include/engine/GraphExecutor.hpp`
- Test: `tests/test_graphexecutor.cpp`

- [ ] **Step 1: 新建状态枚举头**

Create `include/engine/NodeStatus.hpp`:

```cpp
#pragma once

// 单节点执行状态（GraphExecutor::nodeFinished 以 int 传递）
enum class NodeStatus { Ok = 0, Failed = 1, Skipped = 2, Cancelled = 3 };
```

- [ ] **Step 2: 写失败测试（更新 test_graphexecutor 到新契约）**

在 `tests/test_graphexecutor.cpp` 顶部加入 include：

```cpp
#include "engine/NodeStatus.hpp"
```

把 `pipelineRunsInTopologicalOrder()`（约 76-83 行）中：

```cpp
        QTRY_COMPARE(nodeSpy.count(), 3);
        for (const QVariantList& args : nodeSpy) {
            QVERIFY2(args.at(1).toBool(), qPrintable(args.at(2).toString()));
        }
```

改为：

```cpp
        QTRY_COMPARE(nodeSpy.count(), 3);
        for (const QVariantList& args : nodeSpy) {
            QCOMPARE(args.at(1).toInt(), int(NodeStatus::Ok));
            QVERIFY(args.at(3).toInt() >= 0);
        }
```

把另一处（约 122-124 行）的：

```cpp
        QTRY_COMPARE(nodeSpy.count(), 4);
        for (const QVariantList& args : nodeSpy)
```

改为（保留循环体，仅改判断；若循环体是 `QVERIFY2(args.at(1).toBool()...)`）：

```cpp
        QTRY_COMPARE(nodeSpy.count(), 4);
        for (const QVariantList& args : nodeSpy)
            QCOMPARE(args.at(1).toInt(), int(NodeStatus::Ok));
```

把 `errorPropagatesToDownstream()`（约 199-204 行）中：

```cpp
        QCOMPARE(nodeSpy.at(0).at(0).toString(), load.uuid().toString());
        QVERIFY(!nodeSpy.at(0).at(1).toBool());
        QVERIFY(!nodeSpy.at(1).at(1).toBool());
        QVERIFY(!nodeSpy.at(2).at(1).toBool());
        QCOMPARE(nodeSpy.at(1).at(2).toString(), QStringLiteral("上游节点失败"));
```

改为：

```cpp
        QCOMPARE(nodeSpy.at(0).at(0).toString(), load.uuid().toString());
        QCOMPARE(nodeSpy.at(0).at(1).toInt(), int(NodeStatus::Failed));
        QCOMPARE(nodeSpy.at(1).at(1).toInt(), int(NodeStatus::Skipped));
        QCOMPARE(nodeSpy.at(2).at(1).toInt(), int(NodeStatus::Skipped));
        QCOMPARE(nodeSpy.at(1).at(2).toString(), QStringLiteral("上游节点失败"));
```

在类内新增两个测试：

```cpp
    void nodeStartedPrecedesFinishWithDuration() {
        DAGraph g;
        ImageLoadNode load;
        ResizeNode resize;
        ImageShowNode show;
        QVERIFY(g.addNode(&load));
        QVERIFY(g.addNode(&resize));
        QVERIFY(g.addNode(&show));
        QVERIFY(g.addEdge(load.getOutPorts()[0], resize.getInPorts()[0]));
        QVERIFY(g.addEdge(resize.getOutPorts()[0], show.getInPorts()[0]));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath("t.png");
        QVERIFY(cv::imwrite(path.toStdString(), cv::Mat(8, 8, CV_8UC3, cv::Scalar(1, 2, 3))));
        load.setPath(path);

        GraphExecutor ex;
        ex.setGraph(&g);
        QSignalSpy started(&ex, &GraphExecutor::nodeStarted);
        QSignalSpy finished(&ex, &GraphExecutor::nodeFinished);
        QSignalSpy done(&ex, &GraphExecutor::runFinished);

        QVERIFY(ex.run());
        QTRY_VERIFY_WITH_TIMEOUT(done.count() > 0, 10000);
        QTRY_COMPARE(started.count(), 3);
        QTRY_COMPARE(finished.count(), 3);
        QCOMPARE(started.at(0).at(0).toString(), load.uuid().toString());
        for (const QVariantList& a : finished) {
            QCOMPARE(a.at(1).toInt(), int(NodeStatus::Ok));
            QVERIFY(a.at(3).toInt() >= 0);
        }
    }

    void selfCancelReportsCancelled() {
        // 自定义执行器：执行中把 cancel 置位，模拟“运行中被取消”
        struct SelfCancelExecutor : NodeExecutor {
            ExecResult execute(const ExecuteContext& ctx, const QVariantMap&,
                               const QVector<NodeData>&) const override {
                if (ctx.cancel) ctx.cancel->store(true);
                return {true, QString(), {}};
            }
        };
        struct CancelTestNode : BaseNode {
            Q_OBJECT
        public:
            QString typeName() const override { return "CancelTest"; }
            CancelTestNode(QQuickItem* parent = nullptr) : BaseNode(parent) {}
        };
        NodeRegistry::instance().registerExecutor("CancelTest",
                                                  std::make_shared<SelfCancelExecutor>());

        DAGraph g;
        CancelTestNode n;
        QVERIFY(g.addNode(&n));

        GraphExecutor ex;
        ex.setGraph(&g);
        QSignalSpy finished(&ex, &GraphExecutor::nodeFinished);
        QSignalSpy done(&ex, &GraphExecutor::runFinished);
        QVERIFY(ex.run());
        QTRY_VERIFY_WITH_TIMEOUT(done.count() > 0, 10000);
        QVERIFY(!done.takeFirst().at(0).toBool());
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(finished.at(0).at(1).toInt(), int(NodeStatus::Cancelled));
    }
```

- [ ] **Step 3: 运行测试确认失败**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: 编译失败（`nodeFinished` 仍是旧签名 / `NodeStatus` 未使用），测试无法链接。

- [ ] **Step 4: 实现 GraphExecutor 改动**

在 `include/engine/GraphExecutor.hpp` 顶部加入：

```cpp
#include "engine/NodeStatus.hpp"
```

在类内 `signals:` 之前的 `public:` 区域无需新增枚举（使用 `NodeStatus.hpp`）。

把信号声明：

```cpp
    void nodeStarted(const QString& uuid);
    void nodeFinished(const QString& uuid, bool ok, const QString& error);
```

改为：

```cpp
    void nodeStarted(const QString& uuid);
    void nodeFinished(const QString& uuid, int status, const QString& error, int durationMs);
```

把 `worker()` 里的 `reportNode` lambda 替换为下面两个（保留原日志）：

```cpp
        auto reportStarted = [this, runId](const QString& uuid) {
            QMetaObject::invokeMethod(this, [this, runId, uuid] {
                if (runId != m_run_id.load()) return;
                emit nodeStarted(uuid);
            }, Qt::QueuedConnection);
        };

        auto reportNode = [this, runId](const QString& uuid, int status,
                                        const QString& err, int ms) {
            Log::debug(QStringLiteral("节点完成：%1 status=%2 %3")
                           .arg(uuid).arg(status).arg(ms));
            QMetaObject::invokeMethod(this, [this, runId, uuid, status, err, ms] {
                if (runId != m_run_id.load()) return;
                emit nodeFinished(uuid, status, err, ms);
            }, Qt::QueuedConnection);
        };
```

把跳过分支：

```cpp
                reportNode(uuid, false, QStringLiteral("上游节点失败"));
```

改为：

```cpp
                reportNode(uuid, int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
```

把执行段：

```cpp
            ExecResult r;
            auto exec = NodeRegistry::instance().executorFor(ns.type);
            if (!exec) {
                r.ok = false;
                r.error = QStringLiteral("无执行器");
            } else {
                ExecuteContext ctx{ns.uuid, &m_cancel, [](const QString&) {}};
                r = exec->execute(ctx, ns.params, inputs);
            }

            if (!r.ok) {
                failed.insert(n);
                allOk = false;
            }
            cache.insert(n, r.outputs);
            reportNode(uuid, r.ok, r.error);

            if (r.ok) {
```

改为：

```cpp
            reportStarted(uuid);
            const auto nodeT0 = std::chrono::steady_clock::now();
            ExecResult r;
            auto exec = NodeRegistry::instance().executorFor(ns.type);
            if (!exec) {
                r.ok = false;
                r.error = QStringLiteral("无执行器");
            } else {
                ExecuteContext ctx{ns.uuid, &m_cancel, [](const QString&) {}};
                r = exec->execute(ctx, ns.params, inputs);
            }
            const int nodeMs = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - nodeT0).count());

            const bool cancelled = m_cancel.load();
            const int status = cancelled ? int(NodeStatus::Cancelled)
                                         : (r.ok ? int(NodeStatus::Ok) : int(NodeStatus::Failed));
            if (!r.ok) {
                failed.insert(n);
                allOk = false;
            }
            if (status == int(NodeStatus::Ok))
                cache.insert(n, r.outputs);
            reportNode(uuid, status, r.error, nodeMs);

            if (cancelled) { aborted = true; break; }

            if (r.ok) {
```

把环分支：

```cpp
                reportNode(s.uuid, false, QStringLiteral("图中存在环"));
```

改为：

```cpp
                reportNode(s.uuid, int(NodeStatus::Failed), QStringLiteral("图中存在环"), 0);
```

- [ ] **Step 5: 运行测试确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed out of 15`。

- [ ] **Step 6: 提交**

```bash
git add include/engine/NodeStatus.hpp include/engine/GraphExecutor.hpp tests/test_graphexecutor.cpp
git commit -m "feat(engine): per-node status, start signal and timing; skip/cancel semantics"
```

---

## Task 2: ExecQueueModel 列表模型

**Files:**
- Create: `include/ExecQueueModel.hpp`
- Create: `tests/test_execqueue.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: 写失败测试**

Create `tests/test_execqueue.cpp`:

```cpp
#include <QtTest>
#include "ExecQueueModel.hpp"
#include "engine/NodeStatus.hpp"

class TestExecQueue : public QObject {
    Q_OBJECT
private slots:
    void addRunningAppends() {
        ExecQueueModel m;
        QSignalSpy spy(&m, &QAbstractItemModel::rowsInserted);
        m.addRunning("u1", "节点A");
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::UuidRole).toString(), QString("u1"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::NameRole).toString(), QString("节点A"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("running"));
    }

    void finishNodeUpdatesRow() {
        ExecQueueModel m;
        m.addRunning("u1", "节点A");
        m.addRunning("u2", "节点B");
        m.finishNode("u1", int(NodeStatus::Ok), QString(), 12);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("ok"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::MsRole).toInt(), 12);
        QCOMPARE(m.data(m.index(1), ExecQueueModel::StatusRole).toString(), QString("running"));
        QCOMPARE(m.countDone(), 1);
        QCOMPARE(m.countFailed(), 0);
    }

    void finishUnknownUuidIsSafe() {
        ExecQueueModel m;
        m.addRunning("u1", "节点A");
        QVERIFY(!m.finishNode("nope", int(NodeStatus::Failed), QStringLiteral("err"), 3));
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("running"));
    }

    // 跳过节点从不发 nodeStarted，凭 nodeFinished 直接补建终结行
    void addFinishedAppendsTerminalRow() {
        ExecQueueModel m;
        m.addFinished("s1", "边缘检测", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
        QCOMPARE(m.rowCount(), 1);
        QCOMPARE(m.data(m.index(0), ExecQueueModel::UuidRole).toString(), QString("s1"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::NameRole).toString(), QString("边缘检测"));
        QCOMPARE(m.data(m.index(0), ExecQueueModel::StatusRole).toString(), QString("skipped"));
        QCOMPARE(m.countSkipped(), 1);
    }

    void countsStatuses() {
        ExecQueueModel m;
        m.addRunning("a", "A");
        m.addRunning("b", "B");
        m.addRunning("c", "C");
        m.addRunning("d", "D");
        m.finishNode("a", int(NodeStatus::Ok), QString(), 1);
        m.finishNode("b", int(NodeStatus::Failed), QStringLiteral("e"), 2);
        m.finishNode("c", int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
        m.finishNode("d", int(NodeStatus::Cancelled), QString(), 5);
        QCOMPARE(m.countDone(), 4);
        QCOMPARE(m.countFailed(), 1);
        QCOMPARE(m.countSkipped(), 1);
        QCOMPARE(m.countCancelled(), 1);
    }

    void beginRunClears() {
        ExecQueueModel m;
        m.addRunning("u1", "A");
        m.beginRun();
        QCOMPARE(m.rowCount(), 0);
    }
};

QTEST_MAIN(TestExecQueue)
#include "test_execqueue.moc"
```

- [ ] **Step 2: 把触发的头加入 TEST_HEADERS**

在 `tests/CMakeLists.txt` 的 `set(TEST_HEADERS ...)` 列表中加入（放在 `include/node/...` 之前即可）：

```cmake
    ${PROJECT_SOURCE_DIR}/include/ExecQueueModel.hpp
    ${PROJECT_SOURCE_DIR}/include/engine/NodeStatus.hpp
```

- [ ] **Step 3: 运行确认失败**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: 失败，`ExecQueueModel.hpp` 不存在。

- [ ] **Step 4: 实现 ExecQueueModel**

Create `include/ExecQueueModel.hpp`:

```cpp
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
```

- [ ] **Step 5: 运行确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 新增 `test_execqueue` 通过，总数 `100% tests passed out of 16`。

- [ ] **Step 6: 提交**

```bash
git add include/ExecQueueModel.hpp tests/test_execqueue.cpp tests/CMakeLists.txt
git commit -m "feat: add ExecQueueModel for execution queue"
```

---

## Task 3: NodeManager 接线（模型、计数摘要）

**Files:**
- Modify: `include/NodeManager.h`

- [ ] **Step 1: 加入 include 与成员**

在 `include/NodeManager.h` 顶部 include 区加入：

```cpp
#include "ExecQueueModel.hpp"
#include "engine/NodeStatus.hpp"
```

在成员区（`GraphExecutor m_executor;` 附近）加入：

```cpp
    ExecQueueModel m_exec_queue;
    int m_queue_total = 0;
    bool m_queue_has_result = false;
```

- [ ] **Step 2: 替换构造里的信号连接**

把构造函数里：

```cpp
        QObject::connect(&m_executor, &GraphExecutor::nodeFinished, this,
                         [this](const QString& uuid, bool ok, const QString& error) {
            m_node_errors[uuid] = ok ? QString() : error;
            ++m_error_revision;
            emit errorRevisionChanged();
        });
```

替换为：

```cpp
        QObject::connect(&m_executor, &GraphExecutor::nodeStarted, this,
                         [this](const QString& uuid) {
            m_exec_queue.addRunning(uuid, nameOf(uuid));
            emit queueChanged();
        });
        QObject::connect(&m_executor, &GraphExecutor::nodeFinished, this,
                         [this](const QString& uuid, int status, const QString& error, int ms) {
            // 跳过/环等节点从不发 nodeStarted，此时补建终结行
            if (!m_exec_queue.finishNode(uuid, status, error, ms))
                m_exec_queue.addFinished(uuid, nameOf(uuid), status, error, ms);
            m_node_errors[uuid] = (status == int(NodeStatus::Ok)) ? QString() : error;
            ++m_error_revision;
            emit errorRevisionChanged();
            emit queueChanged();
        });
```

并把 `runningChanged` 的连接改为同时处理“运行开始清空队列”：

```cpp
        connect(&m_executor, &GraphExecutor::runningChanged, this, [this] {
            if (m_executor.running()) {
                m_exec_queue.beginRun();
                m_queue_total = nodeCount();
                m_queue_has_result = true;
                emit queueChanged();
            }
            emit engineChanged();
        });
```

- [ ] **Step 3: 加 nameOf 辅助与计数 getter/属性**

在私有区 `refresh()` 附近加入：

```cpp
    QString nameOf(const QString& uuid) const {
        if (!m_paint_board) return uuid;
        for (BaseNode* n : m_paint_board->m_graph.getAllNodes())
            if (n->uuid().toString() == uuid) return n->name();
        return uuid;
    }
```

在 `Q_PROPERTY` 区加入：

```cpp
    Q_PROPERTY(QAbstractListModel* execQueue READ execQueue CONSTANT)
    Q_PROPERTY(bool queueHasResult READ queueHasResult NOTIFY queueChanged)
    Q_PROPERTY(int queueTotal READ queueTotal NOTIFY queueChanged)
    Q_PROPERTY(int queueDone READ queueDone NOTIFY queueChanged)
    Q_PROPERTY(int queueFailed READ queueFailed NOTIFY queueChanged)
    Q_PROPERTY(int queueSkipped READ queueSkipped NOTIFY queueChanged)
    Q_PROPERTY(int queueCancelled READ queueCancelled NOTIFY queueChanged)
```

在 `engineStatus()` 附近的 getter 区加入：

```cpp
    QAbstractListModel* execQueue() { return &m_exec_queue; }
    bool queueHasResult() const { return m_queue_has_result; }
    int queueTotal() const { return m_queue_total; }
    int queueDone() const { return m_exec_queue.countDone(); }
    int queueFailed() const { return m_exec_queue.countFailed(); }
    int queueSkipped() const { return m_exec_queue.countSkipped(); }
    int queueCancelled() const { return m_exec_queue.countCancelled(); }
```

在信号区加入：

```cpp
    void queueChanged();
```

- [ ] **Step 4: 构建验证**

Run: `cmake --build build -j4 2>&1 | tail -3 && ctest --test-dir build --output-on-failure`
Expected: 构建通过，`100% tests passed out of 16`。

- [ ] **Step 5: 提交**

```bash
git add include/NodeManager.h
git commit -m "feat: NodeManager owns execution queue model and summary counters"
```

---

## Task 4: 设置项 queueAnimation

**Files:**
- Modify: `include/Settings.h`
- Modify: `qml/settings/SettingsDialog.qml`
- Test: `tests/test_settings.cpp`

- [ ] **Step 1: 写失败测试**

在 `tests/test_settings.cpp` 的 `private slots:` 内新增：

```cpp
    void queueAnimationDefaultsAndPersists() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QVERIFY(s.queueAnimation());
        QSignalSpy spy(&s, &Settings::queueAnimationChanged);
        s.setQueueAnimation(false);
        QCOMPARE(spy.count(), 1);
        QVERIFY(!s.queueAnimation());
        Settings s2(f.fileName());
        QVERIFY(!s2.queueAnimation());
    }
```

- [ ] **Step 2: 运行确认失败**

Run: `cmake --build build -j4 2>&1 | tail -5`
Expected: 编译失败（`queueAnimation` 未声明）。

- [ ] **Step 3: 在 Settings.h 实现**

参照 `previewFullRes`（`include/Settings.h` 第 22-28/86-92/145-146/267/342-347/382/423 行附近的模式）加入同类成员：

```cpp
    Q_PROPERTY(bool queueAnimation READ queueAnimation WRITE setQueueAnimation NOTIFY queueAnimationChanged)   // 放在其它 Q_PROPERTY 旁
```

```cpp
    bool queueAnimation() const { return m_queueAnimation; }   // getter 区
```

```cpp
    void setQueueAnimation(bool v) {                            // setter 区
        if (m_queueAnimation == v) return;
        m_queueAnimation = v; m_store->setValue("perf/queueAnimation", v); emit queueAnimationChanged();
    }
```

```cpp
    void queueAnimationChanged();                               // 信号区
```

```cpp
    bool m_queueAnimation = true;                               // 成员区
```

```cpp
    m_queueAnimation = true; m_store->setValue("perf/queueAnimation", m_queueAnimation);   // resetDefaults() 内
```

```cpp
    emit queueAnimationChanged();                               // resetDefaults() 内 emit 处
```

```cpp
    m_queueAnimation = m_store->value("perf/queueAnimation", true).toBool();   // load() 内
```

- [ ] **Step 4: 在 SettingsDialog.qml 加开关**

在 `qml/settings/SettingsDialog.qml`：

1. 快照 capture 对象内（`previewFullRes: Settings.previewFullRes,` 旁）加：

```qml
            queueAnimation: Settings.queueAnimation,
```

2. 恢复处（`Settings.previewFullRes = s.previewFullRes` 旁）加：

```qml
        Settings.queueAnimation = s.queueAnimation
```

3. 在 `secPerf`（`property string category: "perf"` 的 Column）里、`画布抗锯齿` 的 `SettingRow` 之后加入：

```qml
                                SettingRow {
                                    title: "运行队列动画"
                                    desc: "状态条运行队列的滑入与状态脉冲动效"
                                    keywords: "队列 动画 queue animation run"
                                    SwitchControl {
                                        checked: Settings.queueAnimation
                                        onToggled: (v) => Settings.queueAnimation = v
                                    }
                                }
```

- [ ] **Step 5: 运行确认通过 + lint**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure && /usr/lib/qt6/bin/qmllint qml/settings/SettingsDialog.qml`
Expected: 测试全通过；`qmllint` 退出码 0。

- [ ] **Step 6: 提交**

```bash
git add include/Settings.h qml/settings/SettingsDialog.qml tests/test_settings.cpp
git commit -m "feat(settings): add queueAnimation toggle"
```

---

## Task 5: NodeManager.focusNode 与画布居中

**Files:**
- Modify: `include/NodeManager.h`
- Modify: `qml/main.qml`

- [ ] **Step 1: 在 NodeManager 加 focusNode 与信号**

在 `NodeManager.h` 的 `Q_INVOKABLE` 区（如 `cancelRun()` 附近）加入：

```cpp
    // 选中节点并请求画布把该节点居中
    Q_INVOKABLE void focusNode(const QString& uuid) {
        if (!m_paint_board) return;
        BaseNode* target = nullptr;
        for (BaseNode* n : m_paint_board->m_graph.getAllNodes()) {
            const bool hit = (n->uuid().toString() == uuid);
            n->setSelected(hit);
            if (hit) { target = n; raiseNode(n); }
        }
        setSelectedNode(target);
        if (target)
            emit nodeFocusRequested(target->x() + target->width() / 2.0,
                                    target->y() + target->height() / 2.0);
        refresh();
    }
```

在信号区加入：

```cpp
    void nodeFocusRequested(qreal wx, qreal wy);
```

- [ ] **Step 2: main.qml 连接信号到画布居中**

在 `qml/main.qml` 的 `StatusBar { ... }` 附近加入：

```qml
    Connections {
        target: NodeManager
        function onNodeFocusRequested(wx, wy) {
            canvas.panX = canvas.width / 2 - wx * canvas.zoom
            canvas.panY = canvas.height / 2 - wy * canvas.zoom
        }
    }
```

- [ ] **Step 3: 构建 + lint**

Run: `cmake --build build -j4 2>&1 | tail -3 && /usr/lib/qt6/bin/qmllint qml/main.qml`
Expected: 构建通过；`qmllint` 退出码 0。

- [ ] **Step 4: 提交**

```bash
git add include/NodeManager.h qml/main.qml
git commit -m "feat: focusNode selects and centers a node from the queue"
```

---

## Task 6: StatusBar 队列 UI、动效与交互

**Files:**
- Modify: `qml/chrome/StatusBar.qml`

- [ ] **Step 1: 加摘要辅助与导入**

在 `qml/chrome/StatusBar.qml` 顶部 import 后补 `import QtQuick.Controls`（已存在可跳过）。在 `id: root` 后加入：

```qml
    readonly property bool queueMode: NodeManager.queueHasResult

    function statusColor(s) {
        if (s === "ok") return Theme.green
        if (s === "failed") return Theme.red
        if (s === "skipped") return Theme.yellow
        if (s === "cancelled") return Theme.fgDim
        return Theme.blue
    }

    function statusGlyph(s) {
        if (s === "ok") return "✓"
        if (s === "failed") return "✕"
        if (s === "skipped") return "–"
        if (s === "cancelled") return "⊘"
        return ""
    }

    function summaryText() {
        var done = NodeManager.queueDone
        var total = NodeManager.queueTotal
        if (NodeManager.engineRunning)
            return "运行中… " + done + "/" + total
        if (NodeManager.queueCancelled > 0)
            return "已取消 " + done + "/" + total
        var t = "完成 " + done + "/" + total
        if (NodeManager.queueFailed > 0) t += " · 失败" + NodeManager.queueFailed
        if (NodeManager.queueSkipped > 0) t += " · 跳过" + NodeManager.queueSkipped
        return t
    }
```

- [ ] **Step 2: 左侧摘要改用队列文案**

把左侧 `Row` 内状态文字：

```qml
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: NodeManager.engineStatus
                color: NodeManager.engineStatus === "失败" ? Theme.red : Theme.fgDim
                font.pixelSize: 11
            }
```

改为：

```qml
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.queueMode ? root.summaryText() : NodeManager.engineStatus
                color: NodeManager.engineStatus === "失败" ? Theme.red : Theme.fgDim
                font.pixelSize: 11
            }
```

- [ ] **Step 3: 用队列替换中间区域**

把中间那段显示「节点/连线/坐标」的 `Row`（`Row { anchors.left: parent.left ... }` 之后、右侧 `Row { anchors.right: parent.right ... }` 之前那一个）整体替换为：

```qml
        // 中间：未运行时显示统计；运行过之后显示运行队列
        Text {
            id: statsText
            visible: !root.queueMode
            anchors.left: parent.left
            anchors.leftMargin: 210
            anchors.verticalCenter: parent.verticalCenter
            text: "节点 " + root.nodeCount + "　连线 " + root.edgeCount + "　坐标[" + root.selectedPos + "]"
            color: Theme.fgDim
            font.pixelSize: 11
        }

        ListView {
            id: queue
            visible: root.queueMode
            anchors.left: parent.left
            anchors.leftMargin: 210
            anchors.right: zoomGroup.left
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            height: root.height
            orientation: ListView.Horizontal
            spacing: 0
            clip: true
            model: NodeManager.execQueue
            interactive: true

            delegate: Item {
                id: del
                required property string uuid
                required property string name
                required property string status
                required property int ms
                required property string error
                required property int index

                height: queue.height
                width: chipRow.implicitWidth

                Row {
                    id: chipRow
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        visible: del.index > 0
                        anchors.verticalCenter: parent.verticalCenter
                        text: "→"
                        color: Theme.fgDim
                        font.pixelSize: 11
                    }

                    Rectangle {
                        id: chip
                        anchors.verticalCenter: parent.verticalCenter
                        height: 22
                        width: chipContent.implicitWidth + 16
                        radius: 11
                        color: del.status === "failed" ? Theme.red : Theme.bg
                        opacity: del.status === "failed" ? 0.12 : 1.0
                        border.width: 1
                        border.color: root.statusColor(del.status)
                        visible: true

                        Row {
                            id: chipContent
                            anchors.centerIn: parent
                            spacing: 6

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: root.statusGlyph(del.status)
                                color: root.statusColor(del.status)
                                font.pixelSize: 11
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: del.name
                                color: Theme.fg
                                font.pixelSize: 11
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                visible: del.ms > 0
                                text: del.ms + "ms"
                                color: Theme.fgDim
                                font.pixelSize: 10
                            }
                        }

                        // 新增滑入动画（受设置门控）
                        transform: Translate { id: chipShift; x: 0 }

                        Component.onCompleted: {
                            if (!Settings.queueAnimation) return
                            chipShift.x = 18
                            chip.opacity = 0
                            slideIn.start()
                        }

                        NumberAnimation {
                            id: slideIn
                            target: chipShift
                            property: "x"
                            from: 18; to: 0
                            duration: 320
                            easing.type: Easing.OutCubic
                        }
                        SequentialAnimation {
                            running: slideIn.running
                            NumberAnimation { target: chip; property: "opacity"; from: 0; to: 1; duration: 240 }
                        }

                        // 运行中状态点脉冲
                        SequentialAnimation on border.width {
                            running: Settings.queueAnimation && del.status === "running"
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 3; duration: 600; easing.type: Easing.InOutSine }
                            NumberAnimation { from: 3; to: 1; duration: 600; easing.type: Easing.InOutSine }
                        }

                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: NodeManager.focusNode(del.uuid)
                        }

                        ToolTip.visible: hoverArea.containsMouse && (del.status === "failed" || del.status === "skipped")
                        ToolTip.text: del.error
                        HoverHandler { id: hoverArea }
                    }
                }
            }

            // 自动跟随：新芯片出现时滚到末尾
            onCountChanged: {
                if (!Settings.queueAnimation) { positionViewAtEnd(); return }
                followAnimation.restart()
            }

            NumberAnimation {
                id: followAnimation
                target: queue
                property: "contentX"
                to: Math.max(0, queue.contentWidth - queue.width)
                duration: 220
                easing.type: Easing.OutCubic
            }
        }
```

> 说明：`ToolTip` 需要 `import QtQuick.Controls`（文件已导入）。`HoverHandler`/`MouseArea` 已由现有节点卡片采用，可用。`Settings` 需 `import Settings`（加入 import 区）。

- [ ] **Step 4: 加 import 与右侧统计**

在 import 区加入：

```qml
import Settings
```

给右侧 Row 一个 id 供 `queue` 定位，并把「节点/连线」并入右侧（替换右侧第一个 `Text`（主题）附近）：

```qml
    Row {
        id: zoomGroup
        anchors.right: parent.right
        anchors.rightMargin: 14
        anchors.verticalCenter: parent.verticalCenter
        spacing: 16
```

在右侧 Row 内、`主题` Text 之前加入（运行过之后显示统计）：

```qml
        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.queueMode
            text: "节点 " + root.nodeCount + " · 连线 " + root.edgeCount
            color: Theme.fgDim
            font.pixelSize: 11
        }
```

> 原右侧 `主题`/`选中`/缩放控件保持不变；`zoomGroup` 这个 id 就是原右侧 Row（只需加 id）。

- [ ] **Step 5: lint**

Run: `/usr/lib/qt6/bin/qmllint qml/chrome/StatusBar.qml`
Expected: 退出码 0（允许既有的 `unqualified` 提示，但无 syntax error）。

- [ ] **Step 6: 构建 + 全量测试**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed out of 16`。

- [ ] **Step 7: 提交**

```bash
git add qml/chrome/StatusBar.qml
git commit -m "feat(ui): execution queue in the status bar with animation and interaction"
```

---

## Task 7: 实机验证（动画、交互、取消、设置门控）

**Files:** 无（验证任务）

- [ ] **Step 1: 启动并跑一张图**

用隔离配置启动（沿用既有做法）：

```bash
cd /home/aimol/Documents/C++/Workspace/Ortdraw
pkill -x main 2>/dev/null
ORTDRAW_SETTINGS_PATH=/tmp/opencode/lt.ini DISPLAY=:0 ./bin/main >/tmp/opencode/run.out 2>&1 &
```

在画布搭一条链（如 加载图片 → 灰度化 → 高斯模糊 → 图像显示），按 F5/「运行」，截图确认：
- 芯片按执行顺序逐个滑入；
- 运行中蓝色脉冲、成功后绿勾 + 耗时；
- 结束后队列保留，左侧显示 `完成 N/N`。

- [ ] **Step 2: 验证交互与设置门控**

- 点击某个芯片 → 该节点被选中且画布居中。
- 设置 → 性能 → 关闭「运行队列动画」→ 重跑：无滑入/脉冲，状态瞬时切换。
- 制造失败（如 加载图片 指向不存在文件）→ 下游显示「跳过」琥珀色，悬停显示「上游节点失败」；运行中点取消 → 在飞节点显示 `⊘`（已取消）。

- [ ] **Step 3: 记录截图证据**

将关键截图存 `/tmp/opencode/queue-*.png`，确认无 QML 运行时错误：

```bash
grep -i "error\|warning" /tmp/ortdraw.log | head
```

- [ ] **Step 4: 提交（如有微调）**

```bash
git add -A && git commit -m "chore: polish execution queue after live verification"
```

---

## Self-Review 记录

- **Spec 覆盖**：信号与数据（Task 1）、队列模型与摘要（Task 2/3）、设置项（Task 4）、UI/动效/交互（Task 6）、点击定位（Task 5）、测试（Task 1/2/4）、风险提示（ListView 手动滚动策略——Task 6 的 `onCountChanged` 重新跟随）。全部有对应任务。
- **占位符**：无 TBD/TODO；每个代码步骤均给出完整代码。
- **类型一致性**：`NodeStatus` 值（Ok/Failed/Skipped/Cancelled）在 Task 1 定义，Task 2/3 复用；`finishNode(uuid, int, QString, int) -> bool` 与 Task 1 的 `nodeFinished(uuid, int, QString, int)` 一致；Task 3 用 `addFinished` 补建“跳过/环”节点的终结行（Important 审查项修复）。模型角色名（uuid/name/status/ms/error）在 Task 2 定义、Task 6 的 delegate 使用一致；`execQueue/queueTotal/queueDone/queueFailed/queueSkipped/queueCancelled/queueHasResult/focusNode` 在 Task 3/5 定义、Task 6 使用一致。
