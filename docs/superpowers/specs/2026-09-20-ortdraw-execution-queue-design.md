# Ortdraw 运行队列（状态条改造）— 设计文档

- 日期：2026-09-20
- 状态：待评审
- 范围：把底部状态条的中间区域改造成**按执行顺序展示节点运行结果的队列**，带状态与单节点耗时、滑入动效、自动跟随、点击定位与失败悬停提示。
- 非目标：自动重算；并行/多线程求值；队列持久化到文件；GPU；改为「提前列出全部节点」的等待态（本设计为逐个出现）。

## 1. 目标

1. 运行图时，节点**按拓扑（执行）顺序**逐个进入状态条队列：出现 → 运行中 → 成功/失败/跳过/已取消。
2. 每个芯片显示：类型图标 + 节点名 + 状态 + **单节点耗时(ms)**。
3. 运行结束后队列**保留**为「最近一次结果」；左侧摘要显示 `完成 5/6 · 失败1` 等。
4. 动效：芯片滑入（~320ms）、运行中状态点脉冲、状态切换过渡；由设置开关统一门控。
5. 交互：点击芯片选中并居中该节点；悬停失败/跳过芯片显示错误原因。
6. 区分「本身失败」与「因上游失败被连累（跳过）」，以及「取消」。

## 2. 现状与问题

- `GraphExecutor` 现有信号只有 `nodeFinished(uuid, ok, error)`、`nodeImageReady`、`runFinished`：**拿不到单节点耗时**，也无法区分 失败/跳过/取消。
- 状态条中间只显示静态的「节点/连线/坐标」，运行过程不可见（只有左侧一个 `运行中…` 文字）。
- 无逐节点「开始」事件，无法在节点真正开始执行时创建「运行中」条目。

## 3. 后端信号与数据契约（`GraphExecutor`）

新增节点状态枚举，供 NodeManager/QML 使用：

```cpp
enum class NodeStatus { Ok = 0, Failed = 1, Skipped = 2, Cancelled = 3 };
Q_ENUM(NodeStatus)
```

信号变更：

```cpp
void nodeStarted(const QString& uuid);
void nodeFinished(const QString& uuid, int status, const QString& error, int durationMs);
void runFinished(bool ok);
```

- `nodeStarted`：worker 中节点**真正开始执行前**发出（`QMetaObject::invokeMethod` 队列入 GUI 线程）。
- `nodeFinished`：
  - 耗时用 `std::chrono::steady_clock` 在 worker 内测单节点执行时间，`durationMs >= 0`；
  - 上游失败 → `Skipped`，`error="上游节点失败"`；
  - 正常完成 → `Ok` / `Failed`；
  - **取消**：节点执行完成后若检测到 `m_cancel`，该节点回报 `Cancelled` 并终止循环，后续节点不再执行/上报。
- `runFinished(ok)`：`ok = 全部成功且未取消`。
- 约束不变：worker 只读快照，不直接访问 `DAGraph`/`QObject`；一切回主线程走队列连接。

## 4. 队列模型（`ExecQueueModel` + `NodeManager`）

新增 `include/ExecQueueModel.hpp`：

```cpp
class ExecQueueModel : public QAbstractListModel {
    // 角色
    enum Roles { UuidRole = Qt::UserRole + 1, NameRole, StatusRole, MsRole, ErrorRole };
    // status 取值（QString）："running" | "ok" | "failed" | "skipped" | "cancelled"
    void beginRun();                                               // 清空
    void addRunning(const QString& uuid, const QString& name);     // 追加 running 行
    void finishNode(const QString& uuid, const QString& status,
                    const QString& error, int ms);                 // 按 uuid 更新最后一行并 dataChanged
};
```

`NodeManager` 持有并接线：

- `Q_PROPERTY(QAbstractListModel* execQueue READ execQueue CONSTANT)`。
- 运行开始（`engineRunning` 变 true）→ `beginRun()`；
  `nodeStarted` → `addRunning(uuid, nameOf(uuid))`；
  `nodeFinished` → `finishNode(...)`（同时保留现有错误表更新与 `errorRevisionChanged`）。
- `nameOf(uuid)`：由图内节点查询（NodeManager 本就有图）。
- **状态摘要**：运行开始时记录**快照节点总数** `queueTotal`（作分母，即使芯片逐个出现）；暴露计数 `queueTotal / queueDone / queueFailed / queueSkipped / queueCancelled`，由 `StatusBar` 拼装文案：
  - 运行中：`运行中… 3/6`
  - 正常结束：`完成 5/6 · 失败1`（有跳过时追加 `· 跳过1`）
  - 取消：`已取消 4/6`
- 结束后**不清空**，保留为最近一次结果；下一次运行开始时清空重建。

## 5. 状态条 UI 与动效（`StatusBar`）

**布局（保持现有高度，不增高）**

- 左：状态圆点 + 摘要文字。
- 中：运行队列 `ListView`（`orientation: Horizontal`，可滚轮横滚），delegate = 「箭头（除首个）+ 芯片」。
- 右：`选中 name · 坐标[x,y]`、主题、缩放（拥挤时低优先级的「主题」先隐藏/省略）。
- **迁移规则**：首次运行之前，中间仍显示原「节点 N · 连线 M · 坐标[x,y]」；从首次运行起，中间改为队列（常驻），`节点 N · 连线 M` 并入右侧显示（不重复展示）。

**芯片视觉**：圆角胶囊 = `类型图标 + 名称 + 状态点 + 耗时(ms)`；状态色：运行中 蓝（状态点脉冲 + 蓝色边框高亮，**无底部扫光**）、成功 绿、失败 红（浅红底）、跳过 琥珀、已取消 灰。

**动效**

- 新增芯片：`opacity 0→1` + `translateX 18→0`，约 320ms `OutCubic`。
- 状态切换：颜色/边框过渡约 250ms；耗时文字淡入。
- 自动跟随：新芯片出现或出现失败/跳过时，对 `contentX` 平滑 `NumberAnimation` 滚到目标；用户手动滚动可临时接管。
- 由 **设置 → 性能 → 「运行队列动画」** 统一门控：关闭时无滑入/脉冲，状态瞬时切换、滚动瞬时。

## 6. 交互

- **点击芯片** → 选中该节点并平移画布居中：`NodeManager` 新增 `Q_INVOKABLE void focusNode(const QString& uuid)`，选中节点后发信号 `nodeFocusRequested(real wx, real wy)`，由 `CanvasArea` 居中（复用 minimap 的 `jumpTo` 机制）。
- **悬停** 失败/跳过芯片 → 主题化 Tooltip 显示错误原因（复用 `Theme` 与现有 Tooltip 风格）。

## 7. 设置项

- `Settings` 新增 `queueAnimation`（bool，默认 `true`），key `perf/queueAnimation`；`resetDefaults`/load/save 同步，新增 `queueAnimationChanged` 信号。
- `SettingsDialog` → 性能 分组新增开关「运行队列动画」，含描述/关键词/快照回滚，沿用 `SwitchControl`。

## 8. 测试

- `test_graphexecutor`（扩展）：
  - 每节点先 `nodeStarted` 再 `nodeFinished`，uuid 与拓扑顺序一致；
  - `durationMs >= 0`；
  - 失败传播：上游失败 → 下游 `Skipped`（error="上游节点失败"）；
  - 取消：`run()` 后立即 `cancel()` → 至少一个节点回报 `Cancelled`，且 `runFinished(false)`；
  - 更新现有依赖旧 `nodeFinished(uuid, bool, ...)` 签名的断言。
- 新增 `test_execqueue`：`addRunning` 追加 `running` 行、`finishNode` 按 uuid 更新且各角色正确、`beginRun` 清空、未知 uuid 的 `finishNode` 安全无操作。
- `test_settings`：`queueAnimation` 默认值与持久化。
- QML：`qmllint`（StatusBar / 模型绑定）。

## 9. 不改动 / 风险

- 不改动 `.ortdraw` 存读档格式。
- 不改动执行引擎的线程/快照模型与 `ImageStore`/缩略图机制。
- 风险：`nodeFinished` 签名变更会影响所有监听点（目前仅 `NodeManager`），需一并更新并跑全量测试；ListView 自动滚动与用户手动滚动需避免互相打断（用「用户滚动后暂停自动跟随，下一次新芯片恢复」策略）。
