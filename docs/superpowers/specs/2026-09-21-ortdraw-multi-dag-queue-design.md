# Ortdraw 多 DAG（多条独立链）运行队列 — 设计文档

- 日期：2026-09-21
- 状态：待评审
- 范围：当一次运行涉及多个互不相连的节点组（多条独立链）时，运行队列能按「组」区分与筛选：状态条提供下拉切换要查看的链，单链时隐藏下拉，「全部组」视图用同组同色边条区分，失败时自动切到出错的链。
- 非目标：多个文档/多标签页；手动划分组；按根节点分组的重叠语义；把队列持久化到文件。

## 1. 目标

1. 识别一次运行中的多个「组」：连通分量（无向：有边相连即同组），在**运行开始时**由图快照计算。
2. 状态条队列支持**按组筛选**：组数 ≥ 2 时出现下拉（`全部组 (2) ▾`），选中某组后队列、左侧汇总只反映该组。
3. 「全部组」视图把同一组的芯片**连续排列**（组内保持执行顺序），组与组之间用**一条同组色竖线**分隔（分隔线数 = 组数 − 1）。
4. 每组有稳定颜色与自动生成的名称（起始节点名 + `…`）。
5. 每次运行**首次**出现失败/跳过时，自动切到该节点所属组，并把该芯片滚入队列可视区（不移动画布）。
6. 单链（最常见）与未运行时**不显示下拉**，行为与现状一致。

## 2. 现状与问题

- 单文档单图（`DAGraph` 无分量概念）。一张画布可含多条互不相连的链。
- `GraphExecutor` 用 Kahn 拓扑排序，把所有入度为 0 的节点按节点顺序入队，多条链的节点在**同一个队列里交叉出现**，无法区分归属。
- `ExecQueueModel` 的行为 `{uuid,name,status,ms,error}`，没有组信息；状态条只有一个扁平队列。

## 3. 数据与信号

### 3.1 组划分（NodeManager，运行开始时，基于快照）

- 输入：当前图的所有节点与边（运行 `run()` 时读取，与执行器快照同源）。
- 算法：并查集/DFS 求**无向连通分量**；每个分量即一组。
- 组编号：以 `getAllNodes()` 的节点顺序为**唯一确定性次序**。每组取其成员中**最小的节点下标**，按该下标升序编号 0,1,2…（保证每次运行下拉项顺序稳定）。
- 组名：取该组内**下标最小的根节点（入度 0）**的名称 + `…`；若组内无根（存在环）则退化为该组下标最小的节点名 + `…`。
- 颜色：从固定调色板按组序号循环取色（见 4.2）。

### 3.2 `ExecQueueModel` 新增角色

- 新增 `GroupRole`（`int`，`-1` 表示未分组/未知）。`roleNames()` 增加 `"group"`。
- `addRunning(uuid, name)` 与 `addFinished(uuid, name, status, error, ms)` 增加 `group` 参数；或提供 `setGroup(uuid, group)`。实现取前者（一次写入）。

### 3.3 `NodeManager` 暴露接口

- `Q_PROPERTY(QVariantList queueGroups ...)`：每项 `{ id:int, name:string, color:QColor, count:int, ok:int, failed:int, skipped:int }`。
- `Q_PROPERTY(int selectedGroup ...)`：`-1` = 全部组；其余为组 id。可写（下拉选择）。
- `Q_PROPERTY(QAbstractListModel* execQueue ...)`：改为返回**过滤代理模型**（`QSortFilterProxyModel` 子类），按 `GroupRole == selectedGroup`（`-1` 时全部）过滤。
- 左侧汇总计数：`queueDone/queueFailed/queueSkipped/queueCancelled` 需**按当前所选组**统计（`-1` 时统计全部）。新增一个内部函数按组统计；`queueTotal` 亦按所选组（`-1` 时为运行快照总节点数）。
- `queueHasResult` 语义不变。

### 3.4 自动切换信号

- 新增信号 `void focusQueueNode(const QString& uuid)`：仅用于让 `StatusBar` 把对应芯片滚入队列可视区（**不**移动画布；与现有的画布居中信号 `nodeFocusRequested` 相互独立）。
- 在 `nodeFinished` 处理里：若状态为 `Failed`/`Skipped` **且本次运行尚未自动切换过**，将 `selectedGroup` 设为该节点的组并发出 `focusQueueNode(uuid)`；设置 `m_autoSwitched = true`（每次运行开始重置）。
- 组信息在运行开始时即算好并发布，运行中到达的 `nodeStarted/nodeFinished` 直接带上组 id。

## 4. 状态条 UI（`StatusBar.qml`）

### 4.1 下拉

- 仅当 `NodeManager.queueHasResult && queueGroups.length >= 2` 时显示，位于队列左侧、`leftGroup` 右侧。
- 按钮：`● 全部组 (2) ▾`（色点为当前所选组色，全部组用中性灰）。
- 弹出菜单项格式（方案 A）：`① 加载图片…` + `N 节点` + 汇总（`完成` / `失败N` / `跳过N`）；首项为「全部组 (N)」。
- 选中项写入 `NodeManager.selectedGroup`。

### 4.2 芯片与「全部组」视图

- 调色板：`[Theme.blue, #8250df, #0d9488, Theme.orange, Theme.magenta]` 循环（具体色以 `Theme` 可用值为准）。
- 选中具体组时：队列只显示该组芯片（代理模型已过滤），无需额外标记。
- 「全部组」时：`execQueue` 代理按 `group` 升序**稳定排序**（组内保持执行顺序）；delegate 在每组的**首个**芯片前画一条 2px 同组色竖线（其余不加），故分隔线数 = 组数 − 1。顺序与边界由 `ExecQueueModel` 的 `groupStart` 角色标记。
- 现有状态色（成功绿/失败红/跳过琥珀/取消灰）、耗时、点击定位、悬停错误提示**保持不变**。

### 4.3 左侧汇总

- 文案随所选组变化：全部组为整体；某组为该组 `完成 ok/total · 失败N · 跳过N`。
- 现有 `summaryText()` 改为读取按组统计后的计数。

## 5. 边界情况

- 单链：组数=1 → 不显示下拉；队列即该组，行为同现状。
- 未运行：无组信息 → 不显示下拉。
- 组内存在环：组名退化规则见 3.1；执行器已把环上节点报 `Failed`。
- 孤立节点（无边）：各自成一组（单节点组）。
- 运行中被编辑/删除节点：不影响本次（基于快照的 uuid/组）。
- `clearGraph()`/加载图：重置组信息、`selectedGroup` 归位到 `-1`、`m_autoSwitched` 复位（并入现有 `resetQueue()`）。

## 6. 设置

- 无需新设置项。下拉可见性由组数自动决定。

## 7. 测试

- 单元（`test_execqueue` 扩展）：`GroupRole` 写入与 `roleNames`；代理模型按 `-1`/具体组过滤行数正确。
- 新增分组单元测试（可放入 `test_graphexecutor` 或新 `test_queuegroups`）：
  - 两条独立链 → 2 组且编号/命名正确；
  - 单链 → 1 组；
  - 含环/孤立节点 → 组数正确，命名退化不崩溃；
  - 组内计数（成功/失败/跳过）正确。
- 实机验证：搭两条链运行 → 下拉出现；切换只显示对应链；制造某链失败 → 自动切到该链并滚动到位；单链时不出现下拉。

## 8. 不改动 / 风险

- 不改动 `.ortdraw` 存读档；不改执行引擎的线程/快照模型。
- 组划分是 NodeManager 的视图层逻辑，不进入执行器。
- 风险：`QSortFilterProxyModel` 与 `execQueue` 的 QML `ListView` 绑定需保持 `CONSTANT` 指针且 `roleNames` 透传正确（proxy 默认透传源模型 roleNames）；组顺序/命名需确定性，避免每次运行下拉项乱序。
