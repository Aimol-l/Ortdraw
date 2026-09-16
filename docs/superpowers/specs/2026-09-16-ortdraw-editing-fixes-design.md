# Ortdraw 编辑功能与内存缺陷修复 — 设计文档

- 日期：2026-09-16
- 状态：已批准（待评审）
- 范围：修复节点编辑器现有的崩溃、正确性、内存安全、跨平台构建缺陷；新增单元测试；重写 README。
- 非目标：不实现节点图执行引擎，不集成 ONNX 推理，不新增深度学习节点（后续独立迭代）。

## 1. 背景与现状

Ortdraw 是基于 Qt6/QML + OpenCV 的节点式图像处理工具的**半成品原型**。当前可完成
"创建节点 → 连线 → 移动 → 删除"的编辑交互，但存在多处缺陷，且没有任何测试。

已确认的技术事实：

- 在 Linux（Qt 6.11.2、OpenCV 5.0、GCC 16）下可成功配置并编译。
- `build/` 是旧路径（`CppWorkspace`）残留的缓存，需重新配置。
- 目录非 git 仓库，本次不执行提交操作。
- Qt Test 与 GoogleTest 均可用；选用 **Qt Test**。

### 关键模块

| 模块 | 文件 | 职责 |
| --- | --- | --- |
| 节点基类 | `include/node/BaseNode.hpp` | `QQuickItem`，持有输入/输出端口 |
| 节点实现 | `include/node/ImageLoad.hpp`、`ImageShow.hpp` | 具体节点 |
| 端口 | `include/port/Port.hpp` | `QObject`，位置、类型、连接标志 |
| 图 | `include/utils/DAGraph.hpp` | 邻接表 + 边集合 + 环检测 |
| 边 | `include/utils/Edge.hpp` | 贝塞尔曲线绘制、命中判定 |
| 画板 | `include/PaintBoard.h` | `QQuickPaintedItem`，绘制边 |
| 命令 | `include/command/*.hpp` | 命令模式 undo/redo |
| 编排 | `include/NodeManager.h` | `QML_SINGLETON`，事件入口 |
| UI | `qml/**` | 主窗口、节点、按钮 |

## 2. 缺陷清单

### 2.1 崩溃与正确性

- **B1** `RemoveNodeCMD::undo()` 为空，删除节点无法撤销。
- **B2** `RemoveNodeCMD::execute()` 将 `m_node = nullptr`，且 undo 注释掉，节点永久丢失。
- **B3** `AddNodeCMD::undo()` 删除节点并置空；若随后 redo，`addNode(nullptr)` 崩溃。
- **B4** `DAGraph::removeNode()` 在 `range-for` 遍历 `m_edges` 时调用 `removeOne`，迭代器失效（UB）。
- **B5** `DAGraph::addEdge()` 直接 `m_adj_list[src->father()]`，若节点不在图中会静默插入，破坏结构。
- **B6** `DAGraph::removeEdge()` 不重置两端端口的 `m_connected`，删除后端口无法再次连线。
- **B7** `NodeManager` 的 `m_paint_board` 未初始化；在 QML 注入前调用会崩溃。
- **B8** `NodeManager::undo()/redo()` 不调用 `m_paint_board->update()`，撤销/重做后界面不刷新。
- **B9** `NodeManager::nodeResizeEvent` 传入的是鼠标位移增量，而非节点宽度增量，端口漂移。
- **B10** `nodeResizeEvent` 只移动输出端口；输入端口在 resize 时不更新（当前输入口右对齐，理论上不受宽度影响，但需统一按"端口锚点"规则处理）。

### 2.2 端口与连线内存安全

- **M1** `NodeManager::setOutputPort` 每次 `new Port()`，取消/异常路径未释放 → 泄漏。
- **M2** `setInputPort` 中 `m_drawing_edge.stop_port->deleteLater()` 后仍可能被 `paint()` 访问 → 悬空风险。
- **M3** 删除节点/边后，`Edge` 中残留的 `Port*` 未保证清理顺序。
- **M4** `Port::m_position` 初始为 `(0,0)`，仅在点击端口时设置；undo/redo 恢复连线时依赖该位置。

### 2.3 编辑体验

- **U1** `removeNode/removeEdge` 只处理第一个选中项，不支持多选删除。
- **U2** `Edge::isPointOnCurve` 只判断 `midPoint` 附近 10px，曲线其余部分不可选中。
- **U3** 点击空白处取消选择的行为与边选择逻辑耦合，且节点命中用 `boundingRect()` 可能与画板坐标不一致。

### 2.4 跨平台构建

- **C1** `src/main.cpp` 的 `#pragma comment(linker, ...)` 为 MSVC 专用，在 GCC/Clang 下无效。
- **C2** `CMakeLists.txt:27` `set(CMAKE_CXX_FLAGS"${...} /std:c++23")` 缺空格，且 `/std:c++23` 不是 MSVC 合法值。
- **C3** `include/NodeManager.cpp` 为空、`include/PaintBoard.cpp` 全为注释；`.cpp` 混在 `include/`，结构混乱。
- **C4** `main.cpp` 同时链接 `Qt6::Widgets`（仅 `QApplication` 用到），可评估是否保留。

### 2.5 测试与文档

- **T1** 无任何测试。
- **D1** `README.md` 是 Codeup 模板，与项目无关。

## 3. 核心设计决策

### 3.1 采用方案 A：删除 = 从图中移除 + 隐藏，不销毁对象

节点由 QML 通过 `Qt.createComponent(...).createObject(canvas)` 创建，其 Qt 对象树所有权
归 QML 画布。因此命令栈**不负责销毁节点**。

- `RemoveNodeCMD::execute()`
  1. 快照该节点关联的所有 `Edge`（双向：出边与入边）及其对端 `Port*`。
  2. 调用 `graph.removeNode(node)` 从邻接表与边集合中摘除。
  3. `node->setVisible(false)`，并清除选中状态。
- `RemoveNodeCMD::undo()`
  1. `graph.addNode(node)` 重新挂回。
  2. 用快照重建所有边（含恢复两端 `m_connected`）。
  3. `node->setVisible(true)`。
- `AddNodeCMD::undo()`
  1. `graph.removeNode(node)` + `node->setVisible(false)`，**不 delete**。
- `AddNodeCMD::execute()`：`graph.addNode(node)` + `node->setVisible(true)`。

命令栈对节点的引用统一改为 `QPointer<BaseNode>`，在对象被 QML 销毁时自动置空，并在
`execute/undo` 入口做空守卫。

> 生命周期约定：节点对象存活时间 = 画布存活时间；"删除"仅是逻辑删除。

### 3.2 临时连线端口由 PaintBoard 持有

`PaintBoard` 新增：

```cpp
std::unique_ptr<Port> m_tmp_port;   // 拖拽连线时的临时终点端口，复用，不每次 new
```

- 开始连线时 `m_tmp_port = std::make_unique<Port>(...)`（或首次分配后复用）。
- 取消连线统一调用 `PaintBoard::cancelDrawing()`：重置 `m_drawing_line`、清空 `m_drawing_edge`、
  释放/复位临时端口。
- 所有失败分支（已连接、自连、类型不匹配、成环）都走 `cancelDrawing()`，消除 `deleteLater` 悬空。
- 成功建立边后同样复位临时端口与 `m_drawing_edge`，保证 `paint()` 不再访问临时对象。

### 3.3 DAGraph 正确性

- `removeNode()`：先遍历边集收集待删边到临时列表，再统一删除，避免迭代器失效。
- `addEdge()`：入口校验 `src`、`dst` 非空且两端节点均已在邻接表中；否则返回 `false`。
- `removeEdge()`：删除成功后设置 `src->m_connected = false`、`dst->m_connected = false`。
- `addEdge()`：成功时设置两端 `m_connected = true`（由 C++ 统一负责，不再散落在 `setInputPort`）。
- 额外校验：一个输入端口只允许一条入边（当前 `m_connected` 已部分承担该职责，收敛到 `addEdge`）。
- `hasCycle()`：保持现有算法，补充对"节点不在图中"的防御。

### 3.4 命令栈

- `CmdManager::executeCommand()`：保留"执行失败不入栈"与"截断 redo 历史"逻辑；
  修正 `redo()` 边界判断（`m_cmd_idx + 1 < size()` 正确，统一类型为 `std::ptrdiff_t`）。
- `NodeManager::undo()/redo()` 之后统一 `m_paint_board->update()`。
- 每次命令执行/撤销/重做后刷新画板，由 `NodeManager` 统一封装 `refresh()`。

### 3.5 端口位置与 resize

- `NodeManager::nodeResizeEvent(uid, dw, dh)` 的 `dw/dh` 语义明确为**节点宽高增量**。
  QML 侧传入 `root.width - prevWidth` / `root.height - prevHeight`。
- 端口锚点规则：
  - 输入端口圆点位于节点左缘，x 不随宽度变化；
  - 输出端口圆点位于节点右缘，x 随宽度增量 `dw` 平移。
- 新增 `BaseNode::syncPortPositions()`，在节点首次布局完成后由 QML 调用，将各端口锚点坐标
  回传 C++，避免依赖"必须点击端口才有正确坐标"。

### 3.6 选择与命中

- `Edge::isPointOnCurve()`：对三次贝塞尔曲线按参数 `t` 采样（如 24 段），计算点到折线段的
  最短距离，阈值 8px。
- `NodeManager::removeNode()`：遍历所有选中节点，逐个入命令栈（各自独立可撤销）。
- `NodeManager::removeEdge()`：同上处理所有选中边。
- 点击空白区域的取消选择逻辑保留，但先处理端口/节点命中，再处理边命中。

### 3.7 跨平台构建

- 删除 `main.cpp` 的 `#pragma comment`；Windows GUI 子系统依赖 CMake 的 `WIN32`（已配置）。
- `CMakeLists.txt`：
  - 修正 `CMAKE_CXX_FLAGS` 拼接（改为 `target_compile_options`）。
  - 标准使用 `target_compile_features(main PRIVATE cxx_std_23)`。
  - Windows 分支用 `/std:c++latest` 替代无效的 `/std:c++23`。
  - 将 `include/*.cpp` 的实现文件迁至 `src/`，删除空文件。
  - 统一 `find_package(Qt6 COMPONENTS Core Quick Test ...)`。
  - 新增 `enable_testing()` 与 `tests/` 子目标。
- 评估移除 `Qt6::Widgets`：若 `QApplication` 可替换为 `QGuiApplication` 则去掉 Widgets 依赖；
  否则保留并注明原因。

### 3.8 测试（Qt Test + CTest）

新增 `tests/`：

- `test_dagraph.cpp`
  - `addNode` 重复添加返回 false。
  - `addEdge` 成功并设置两端 `m_connected`。
  - 自环、间接环被拒绝且图状态不变。
  - `addEdge` 传入未加入图的节点返回 false。
  - `removeEdge` 重置 `m_connected`。
  - `removeNode` 清理关联出/入边，且不触发迭代器失效（删除多个节点）。
- `test_cmdmanager.cpp`
  - AddNode → undo → redo 序列后图状态与可见性正确。
  - RemoveNode 快照恢复：删除含两条边的节点，undo 后节点与两条边均恢复，端口 `m_connected` 正确。
  - AddEdge → undo 后可再次连线。
  - redo 历史截断行为。
- `test_port.cpp`
  - `movedeltaPos`、连接标志、`dataType`。

测试使用 `QTest` 的 `QTEST_MAIN`（`QGuiApplication`），通过 CTest 注册。

### 3.9 README

重写为中文项目说明：项目定位、功能现状（明确标注"编辑功能可用、执行引擎未实现"）、
架构与目录说明、依赖、构建、运行、测试命令、已知限制与后续路线。

## 4. 交付物

1. 修复后的源文件（`include/**`、`src/**`、`qml/**`、`CMakeLists.txt`）。
2. 新增 `tests/` 与 CTest 集成。
3. 重写的 `README.md`。
4. 本设计文档。

## 5. 验证方式

- `cmake -S . -B build && cmake --build build` 在 Linux 通过。
- `ctest --test-dir build --output-on-failure` 全部通过。
- 手工冒烟：创建 ImageLoad/ImageShow 节点 → 连线 → 移动 → 删除节点 → undo（节点与边恢复）
  → redo → 删除边 → 端口可重新连线。

## 6. 风险与取舍

- 被删除节点常驻内存是方案 A 的已知取舍，符合当前规模。
- Windows 构建无法在本机验证，仅做静态修复与条件编译；文档中标注未验证。
- `Qt6::Widgets` 依赖能否移除取决于 `QApplication` 是否可换 `QGuiApplication`，
  将在实现时确认，若不能则保留。
