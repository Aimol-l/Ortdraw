# Ortdraw 编辑功能与内存缺陷修复 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 Ortdraw 节点编辑器在撤销/重做、删除、连线、端口生命周期与跨平台构建上的缺陷，并补齐单元测试与 README。

**Architecture:** 采用"删除 = 从图中移除 + 隐藏，不销毁对象"的方案 A：节点生命周期归 QML 对象树，命令栈只持 `QPointer` 弱引用，`RemoveNodeCMD` 在删除前快照关联边并在 undo 时恢复。临时连线端口由 `PaintBoard` 以 `unique_ptr` 持有并复用。底层数据结构（Port/DAGraph/CmdManager）用 Qt Test 做 TDD。

**Tech Stack:** C++23、CMake 3.30、Qt 6.11（Core/Gui/Quick/Test）、OpenCV 5.0、Qt Test + CTest。

---

## 环境与约定

- 工作目录：`/home/aimol/Documents/C++/Workspace/Ortdraw`
- 该目录**不是 git 仓库**。计划中的 Commit 步骤标记为"待用户授权"，默认不执行 `git init`/commit。
- 所有构建/测试命令都在项目根目录执行，使用独立构建目录 `build/`（先删除旧缓存，旧缓存指向已失效的 `CppWorkspace` 路径）。
- 提交信息风格：`fix: ...` / `test: ...` / `docs: ...`（若授权提交）。

---

## 文件结构

**修改：**
- `CMakeLists.txt` — 修正跨平台编译选项、增加测试子目录、去掉空 `.cpp` 通配
- `src/main.cpp` — 移除 Windows 专用 `#pragma`，改用 `QGuiApplication`
- `include/port/Port.hpp` — 连接状态封装
- `include/utils/DAGraph.hpp` — 增删校验、`edgesOf`、迭代器安全
- `include/utils/Edge.hpp` — 曲线采样命中
- `include/node/BaseNode.hpp` — 端口位置同步接口
- `include/node/ImageLoad.hpp` — 去掉多余输入端口
- `include/PaintBoard.h` — 临时端口持有与连线生命周期
- `include/command/AddNode.hpp`、`RemoveNode.hpp` — 方案 A 所有权
- `include/command/CmdManager.hpp` — 边界返回
- `include/NodeManager.h` — 事件入口重构
- `qml/main.qml`、`qml/node/ImageLoadNode.qml`、`qml/node/ImageShowNode.qml` — 端口位置同步、Ctrl 多选、undo/redo 快捷键
- `README.md` — 重写

**删除：**
- `include/NodeManager.cpp`（空文件）
- `include/PaintBoard.cpp`（全为注释）

**新建：**
- `tests/CMakeLists.txt`
- `tests/test_port.cpp`
- `tests/test_dagraph.cpp`
- `tests/test_cmdmanager.cpp`

---

## Task 1: 构建基线与测试脚手架

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/main.cpp`
- Delete: `include/NodeManager.cpp`, `include/PaintBoard.cpp`
- Create: `tests/CMakeLists.txt`
- Create: `tests/test_port.cpp`

- [ ] **Step 1: 删除失效的空实现文件**

```bash
rm -f include/NodeManager.cpp include/PaintBoard.cpp
```

- [ ] **Step 2: 重写 `CMakeLists.txt`**

```cmake
cmake_minimum_required(VERSION 3.30)
project(
    main
    LANGUAGES CXX C
    DESCRIPTION "基于节点的深度学习图像处理工具"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_FILE} DIRECTORY)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_DIR}/bin)
set(CMAKE_BUILD_PARALLEL_LEVEL 4)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "current platform: Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "current platform: Windows")
    set(OpenCV_DIR "D:/OpenCV/build/x64/vc16/lib")
    set(ONNX_DIR "D:/OnnxRuntime")
    set(TBB_DIR "D:/Tbb/lib/cmake/tbb")
    include_directories(${ONNX_DIR}/include)
    link_directories(${ONNX_DIR}/lib)
    add_compile_options(/W4 /Zc:__cplusplus /std:c++latest)
endif()

find_package(OpenCV REQUIRED)
find_package(Qt6 COMPONENTS Core Gui Quick Test REQUIRED)
qt_standard_project_setup(REQUIRES 6.8)
link_directories(${PROJECT_DIR}/lib)
include_directories(${PROJECT_DIR}/include/)

file(GLOB SRC_FILES
    CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/*.h"
    "${PROJECT_SOURCE_DIR}/include/node/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/port/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/utils/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/command/*.hpp"
    "${PROJECT_SOURCE_DIR}/include/command/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

qt_add_executable(${CMAKE_PROJECT_NAME}
    WIN32
    ./assets.qrc
    ${SRC_FILES}
)
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_23)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE
    ${OpenCV_LIBRARIES}
    Qt6::Core Qt6::Gui Qt6::Quick
)

enable_testing()
add_subdirectory(tests)
```

- [ ] **Step 3: 修改 `src/main.cpp`**

```cpp
#include "NodeManager.h"
#include "PaintBoard.h"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]){
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    const QUrl url("qrc:/main.qml");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    qmlRegisterType<PaintBoard>("PaintBoard", 1, 0, "PaintBoard");
    qmlRegisterType<ImageLoadNode>("ImageLoadNode", 1, 0, "ImageLoadNode");
    qmlRegisterType<ImageShowNode>("ImageShowNode", 1, 0, "ImageShowNode");
    qmlRegisterSingletonInstance("NodeManager", 1, 0, "NodeManager", NodeManager::instance());
    engine.load(url);
    return app.exec();
}
```

- [ ] **Step 4: 新建 `tests/CMakeLists.txt`**

```cmake
set(TEST_HEADERS
    ${PROJECT_SOURCE_DIR}/include/NodeManager.h
    ${PROJECT_SOURCE_DIR}/include/PaintBoard.h
    ${PROJECT_SOURCE_DIR}/include/node/BaseNode.hpp
    ${PROJECT_SOURCE_DIR}/include/node/ImageLoad.hpp
    ${PROJECT_SOURCE_DIR}/include/node/ImageShow.hpp
    ${PROJECT_SOURCE_DIR}/include/port/Port.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/DAGraph.hpp
    ${PROJECT_SOURCE_DIR}/include/utils/Edge.hpp
    ${PROJECT_SOURCE_DIR}/include/command/Command.h
    ${PROJECT_SOURCE_DIR}/include/command/AddNode.hpp
    ${PROJECT_SOURCE_DIR}/include/command/AddEdge.hpp
    ${PROJECT_SOURCE_DIR}/include/command/RemoveNode.hpp
    ${PROJECT_SOURCE_DIR}/include/command/RemoveEdge.hpp
    ${PROJECT_SOURCE_DIR}/include/command/CmdManager.hpp
)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp")
foreach(src ${TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    qt_add_executable(${name} ${src} ${TEST_HEADERS})
    target_include_directories(${name} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${OpenCV_INCLUDE_DIRS}
    )
    target_compile_features(${name} PRIVATE cxx_std_23)
    target_link_libraries(${name} PRIVATE
        Qt6::Core Qt6::Gui Qt6::Quick Qt6::Test
        ${OpenCV_LIBRARIES}
    )
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endforeach()
```

- [ ] **Step 5: 新建 `tests/test_port.cpp`（当前会编译失败，因为 `isConnected` 尚未定义）**

```cpp
#include <QtTest>
#include "port/Port.hpp"
#include "node/BaseNode.hpp"

class TestPort : public QObject {
    Q_OBJECT
private slots:
    void testMoveDelta() {
        Port p("p", PortType::Input, DataType::Image, QPointF(0, 0), nullptr);
        p.movedeltaPos(QPointF(10, -5));
        QCOMPARE(p.position(), QPointF(10, -5));
    }
    void testConnectedFlag() {
        Port p("p", PortType::Input, DataType::Image, QPointF(0, 0), nullptr);
        QVERIFY(!p.isConnected());
        p.setConnected(true);
        QVERIFY(p.isConnected());
    }
    void testDataType() {
        Port p("p", PortType::Output, DataType::Float, QPointF(1, 2), nullptr);
        QCOMPARE(p.dataType(), DataType::Float);
        QCOMPARE(p.type(), static_cast<int>(PortType::Output));
    }
};

QTEST_MAIN(TestPort)
#include "test_port.moc"
```

- [ ] **Step 6: 运行验证 — 主程序构建通过，测试编译失败（预期）**

Run:
```bash
rm -rf build && cmake -S . -B build && cmake --build build -j4
```
Expected: `main` 目标链接成功；`test_port` 目标编译失败，报错包含 `'isConnected' was not declared`。这正是 Task 2 要消除的失败。

---

## Task 2: Port 连接状态封装

**Files:**
- Modify: `include/port/Port.hpp`
- Test: `tests/test_port.cpp`

- [ ] **Step 1: 修改 `include/port/Port.hpp` 的连接标志为私有并提供访问器**

将原第 39 行的公有成员：
```cpp
    bool m_connected = false;
```
移动到 `private:` 区域，并替换为：
```cpp
private:
    bool m_connected = false;
public:
    bool isConnected() const { return m_connected; }
    void setConnected(bool connected) { m_connected = connected; }
```
> 说明：`m_position`、`m_father` 等原有私有成员保留不动；最终 `private:` 段应包含 `m_uid, m_name, m_type, m_data_type, m_position, m_father, m_connected`，其后的 `public:` 段包含构造函数、getter、`isConnected/setConnected`。

- [ ] **Step 2: 运行 Port 测试**

Run:
```bash
cmake --build build -j4 --target test_port && ctest --test-dir build -R test_port --output-on-failure
```
Expected: `test_port` 编译通过，3 个用例 PASS。

- [ ] **Step 3: 处理其他引用 `m_connected` 的编译错误**

此时 `DAGraph.hpp`、`NodeManager.h` 会因仍直接访问 `m_connected` 而编译失败。这些将在 Task 3、Task 7 修复。为使本 Task 结束时主程序可编译，先在 `DAGraph.hpp` 中把 `edge.stop_port->m_connected = false;` 改为 `edge.stop_port->setConnected(false);`，把 `edge.start_port->m_connected = false;` 改为 `edge.start_port->setConnected(false);`。`NodeManager.h` 的 `port->m_connected = true;` 改为 `port->setConnected(true);`。

- [ ] **Step 4: 全量构建验证**

Run:
```bash
cmake --build build -j4
```
Expected: 全部目标构建成功。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add include/port/Port.hpp include/utils/DAGraph.hpp include/NodeManager.h tests/
git commit -m "fix: encapsulate Port connection state"
```

---

## Task 3: DAGraph 增删校验与迭代器安全

**Files:**
- Modify: `include/utils/DAGraph.hpp`
- Test: `tests/test_dagraph.cpp`

- [ ] **Step 1: 新建 `tests/test_dagraph.cpp`**

```cpp
#include <QtTest>
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "port/Port.hpp"

static BaseNode* makeNode() { return new BaseNode(); }

static Port* addPort(BaseNode* n, PortType t, DataType d) {
    auto* p = new Port("p", t, d, QPointF(0, 0), n);
    if (t == PortType::Input) n->getInPorts().append(p);
    else n->getOutPorts().append(p);
    return p;
}

class TestDAGraph : public QObject {
    Q_OBJECT
private slots:
    void addNodeRejectsDuplicate() {
        DAGraph g;
        auto* n = makeNode();
        QVERIFY(g.addNode(n));
        QVERIFY(!g.addNode(n));
        delete n;
    }
    void addEdgeSetsConnected() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(out, in));
        QVERIFY(out->isConnected());
        QVERIFY(in->isConnected());
        QCOMPARE(g.getAllEdges().size(), 1);
        delete a; delete b;
    }
    void addEdgeRejectsCycle() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* a_in = addPort(a, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(!g.addEdge(b_out, a_in));
        QCOMPARE(g.getAllEdges().size(), 1);
        delete a; delete b;
    }
    void addEdgeRejectsUnknownNode() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(!g.addEdge(out, in));
        delete a; delete b;
    }
    void addEdgeRejectsTypeMismatch() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Float);
        QVERIFY(!g.addEdge(out, in));
        delete a; delete b;
    }
    void removeEdgeResetsConnected() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in = addPort(b, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(out, in));
        QVERIFY(g.removeEdge(out, in));
        QVERIFY(!out->isConnected());
        QVERIFY(!in->isConnected());
        QCOMPARE(g.getAllEdges().size(), 0);
        delete a; delete b;
    }
    void removeNodeClearsEdges() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode(); auto* c = makeNode();
        g.addNode(a); g.addNode(b); g.addNode(c);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* c_in = addPort(c, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(g.addEdge(b_out, c_in));
        QVERIFY(g.removeNode(b));
        QCOMPARE(g.getAllEdges().size(), 0);
        QCOMPARE(g.getAllNodes().size(), 2);
        delete a; delete b; delete c;
    }
    void edgesOfCollectsIncidentEdges() {
        DAGraph g;
        auto* a = makeNode(); auto* b = makeNode(); auto* c = makeNode();
        g.addNode(a); g.addNode(b); g.addNode(c);
        auto* a_out = addPort(a, PortType::Output, DataType::Image);
        auto* b_in = addPort(b, PortType::Input, DataType::Image);
        auto* b_out = addPort(b, PortType::Output, DataType::Image);
        auto* c_in = addPort(c, PortType::Input, DataType::Image);
        QVERIFY(g.addEdge(a_out, b_in));
        QVERIFY(g.addEdge(b_out, c_in));
        QCOMPARE(g.edgesOf(b).size(), 2);
        QCOMPARE(g.edgesOf(a).size(), 1);
        delete a; delete b; delete c;
    }
};

QTEST_MAIN(TestDAGraph)
#include "test_dagraph.moc"
```

- [ ] **Step 2: 运行测试确认失败**

Run:
```bash
cmake --build build -j4 --target test_dagraph 2>&1 | tail -20
```
Expected: 编译失败，`'edgesOf' was not declared`（`edgesOf` 尚未实现）。

- [ ] **Step 3: 重写 `include/utils/DAGraph.hpp` 的核心方法**

用以下实现替换 `addNode`、`addEdge`、`removeEdge`、`removeNode`，并在 `public:` 末尾新增 `edgesOf`。`hasCycle` 保持不变。

```cpp
    bool addNode(BaseNode* node){
        if(!node || m_adj_list.contains(node)) return false;
        m_adj_list[node] = QList<BaseNode*>();
        return true;
    }
    bool addEdge(Port* src, Port* dst) {
        if(!src || !dst || !src->father() || !dst->father()) return false;
        if(!m_adj_list.contains(src->father()) || !m_adj_list.contains(dst->father())) return false;
        if(src->father() == dst->father()) return false;
        if(src->dataType() != dst->dataType()) return false;
        if(dst->isConnected()) return false;
        for(const auto& edge : m_edges){
            if(edge.start_port == src && edge.stop_port == dst) return false;
        }
        m_adj_list[src->father()].append(dst->father());
        QHash<BaseNode*, bool> visited;
        QHash<BaseNode*, bool> stack;
        for(auto &node : m_adj_list.keys()){
            if(!visited[node] && hasCycle(node, visited, stack)){
                m_adj_list[src->father()].removeOne(dst->father());
                return false;
            }
        }
        m_edges.append(Edge{src, dst});
        src->setConnected(true);
        dst->setConnected(true);
        return true;
    }
    bool removeEdge(Port* src, Port* dst) {
        for(auto it = m_edges.begin(); it != m_edges.end(); ++it){
            if(it->start_port == src && it->stop_port == dst){
                m_edges.erase(it);
                if(auto jt = m_adj_list.find(src->father()); jt != m_adj_list.end())
                    jt.value().removeOne(dst->father());
                src->setConnected(false);
                dst->setConnected(false);
                return true;
            }
        }
        return false;
    }
    bool removeNode(BaseNode* node) {
        if(!node || !m_adj_list.contains(node)) return false;
        QVector<Edge> incident = edgesOf(node);
        for(const Edge& edge : incident)
            removeEdge(edge.start_port, edge.stop_port);
        m_adj_list.remove(node);
        for(auto &neighbors : m_adj_list)
            neighbors.removeAll(node);
        return true;
    }
    QVector<Edge> edgesOf(BaseNode* node) const {
        QVector<Edge> result;
        for(const auto& edge : m_edges){
            if(edge.start_port->father() == node || edge.stop_port->father() == node)
                result.append(edge);
        }
        return result;
    }
```

- [ ] **Step 4: 运行 DAGraph 测试**

Run:
```bash
cmake --build build -j4 --target test_dagraph && ctest --test-dir build -R test_dagraph --output-on-failure
```
Expected: 8 个用例全部 PASS。

- [ ] **Step 5: 全量构建验证**

Run:
```bash
cmake --build build -j4
```
Expected: 构建成功。

- [ ] **Step 6: Commit（待用户授权）**

```bash
git add include/utils/DAGraph.hpp tests/test_dagraph.cpp
git commit -m "fix: harden DAGraph add/remove and add edgesOf"
```

---

## Task 4: CmdManager 边界与返回值

**Files:**
- Modify: `include/command/CmdManager.hpp`
- Test: `tests/test_cmdmanager.cpp`

- [ ] **Step 1: 新建 `tests/test_cmdmanager.cpp`**

```cpp
#include <QtTest>
#include "command/CmdManager.hpp"
#include "command/AddNode.hpp"
#include "command/RemoveNode.hpp"
#include "command/AddEdge.hpp"
#include "command/RemoveEdge.hpp"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class TestCmdManager : public QObject {
    Q_OBJECT
private:
    PaintBoard* board = nullptr;
    BaseNode* nodeA = nullptr;
    BaseNode* nodeB = nullptr;
    BaseNode* nodeC = nullptr;
    Port* outA = nullptr;
    Port* inB = nullptr;

    void setup() {
        board = new PaintBoard();
        nodeA = new BaseNode();
        nodeB = new BaseNode();
        nodeC = new BaseNode();
        outA = new Port("out", PortType::Output, DataType::Image, QPointF(0, 0), nodeA);
        inB  = new Port("in",  PortType::Input,  DataType::Image, QPointF(0, 0), nodeB);
        nodeA->getOutPorts().append(outA);
        nodeB->getInPorts().append(inB);
    }
    void teardown() {
        board->m_graph = DAGraph{};
        delete nodeA; delete nodeB; delete nodeC; delete board;
        nodeA = nodeB = nodeC = nullptr;
        outA = inB = nullptr;
    }
private slots:
    void init() { setup(); }
    void cleanup() { teardown(); }

    void addNodeUndoRedoKeepsObjectAlive() {
        CmdManager m;
        QVERIFY(m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board)));
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QVERIFY(nodeA->isVisible());
        QVERIFY(m.undo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 0);
        QVERIFY(!nodeA->isVisible());
        QVERIFY(m.redo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QVERIFY(nodeA->isVisible());
    }
    void removeNodeRestoresEdges() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(m.executeCommand(std::make_unique<AddEdgeCMD>(outA, inB, board)));
        QCOMPARE(board->m_graph.getAllEdges().size(), 1);
        QVERIFY(m.executeCommand(std::make_unique<RemoveNodeCMD>(nodeB, board)));
        QCOMPARE(board->m_graph.getAllNodes().size(), 1);
        QCOMPARE(board->m_graph.getAllEdges().size(), 0);
        QVERIFY(!inB->isConnected());
        QVERIFY(m.undo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 2);
        QCOMPARE(board->m_graph.getAllEdges().size(), 1);
        QVERIFY(inB->isConnected());
        QVERIFY(outA->isConnected());
    }
    void addEdgeUndoAllowsReconnect() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(m.executeCommand(std::make_unique<AddEdgeCMD>(outA, inB, board)));
        QVERIFY(m.undo());
        QVERIFY(!inB->isConnected());
        QVERIFY(board->m_graph.addEdge(outA, inB));
    }
    void redoHistoryTruncatedByNewCommand() {
        CmdManager m;
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeA, board));
        m.executeCommand(std::make_unique<AddNodeCMD>(nodeB, board));
        QVERIFY(!m.canRedo());
        QVERIFY(m.undo());
        QVERIFY(m.canRedo());
        QVERIFY(m.executeCommand(std::make_unique<AddNodeCMD>(nodeC, board)));
        QVERIFY(!m.canRedo());
        QCOMPARE(board->m_graph.getAllNodes().size(), 2);
    }
    void undoRedoOnEmptyStackReturnsFalse() {
        CmdManager m;
        QVERIFY(!m.undo());
        QVERIFY(!m.redo());
        QVERIFY(!m.canUndo());
        QVERIFY(!m.canRedo());
    }
};

QTEST_MAIN(TestCmdManager)
#include "test_cmdmanager.moc"
```

- [ ] **Step 2: 运行测试确认失败**

Run:
```bash
cmake --build build -j4 --target test_cmdmanager 2>&1 | tail -20
```
Expected: 编译失败，报错包含 `'canRedo' was not declared`、`'canUndo' was not declared`，且 `undo/redo` 返回 `void` 无法用于 `QVERIFY`。

- [ ] **Step 3: 重写 `include/command/CmdManager.hpp`**

```cpp
#pragma once
#include <cstddef>
#include <memory>
#include <vector>
#include "Command.h"

class CmdManager {
private:
    std::ptrdiff_t m_cmd_idx = -1;
    std::vector<std::unique_ptr<Command>> m_commands;
public:
    CmdManager() = default;
    ~CmdManager() = default;

    bool executeCommand(std::unique_ptr<Command> command) {
        if(!command) return false;
        if(m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size()))
            m_commands.erase(m_commands.begin() + m_cmd_idx + 1, m_commands.end());
        if(!command->execute()) return false;
        m_commands.push_back(std::move(command));
        ++m_cmd_idx;
        return true;
    }
    bool undo() {
        if(m_cmd_idx >= 0){
            m_commands.at(m_cmd_idx)->undo();
            --m_cmd_idx;
            return true;
        }
        return false;
    }
    bool redo() {
        if(m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size())){
            ++m_cmd_idx;
            return m_commands.at(m_cmd_idx)->execute();
        }
        return false;
    }
    bool canUndo() const { return m_cmd_idx >= 0; }
    bool canRedo() const {
        return m_cmd_idx + 1 < static_cast<std::ptrdiff_t>(m_commands.size());
    }
};
```

- [ ] **Step 4: 运行测试确认通过**

Run:
```bash
cmake --build build -j4 --target test_cmdmanager && ctest --test-dir build -R test_cmdmanager --output-on-failure
```
Expected: 5 个用例全部 PASS。此时若 `AddNodeCMD`/`RemoveNodeCMD` 仍是旧实现，`addNodeUndoRedoKeepsObjectAlive` 与 `removeNodeRestoresEdges` 会失败或崩溃——这是 Task 5 的目标。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add include/command/CmdManager.hpp tests/test_cmdmanager.cpp
git commit -m "fix: make CmdManager undo/redo return status"
```

---

## Task 5: 命令所有权改造（方案 A）

**Files:**
- Modify: `include/command/AddNode.hpp`
- Modify: `include/command/RemoveNode.hpp`
- Test: `tests/test_cmdmanager.cpp`（已在 Task 4 建立）

- [ ] **Step 1: 重写 `include/command/AddNode.hpp`**

```cpp
#pragma once

#include <QPointer>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class AddNodeCMD: public Command {
private:
    QPointer<BaseNode> m_node;
    PaintBoard* m_paint_board;
public:
    ~AddNodeCMD() = default;
    AddNodeCMD(BaseNode* node, PaintBoard* paint_board)
        :m_node(node), m_paint_board(paint_board) {}

    bool execute() override {
        if(!m_node || !m_paint_board) return false;
        if(!m_paint_board->m_graph.addNode(m_node)) return false;
        m_node->setVisible(true);
        return true;
    }
    void undo() override {
        if(!m_node || !m_paint_board) return;
        m_paint_board->m_graph.removeNode(m_node);
        m_node->setSelected(false);
        m_node->setVisible(false);
    }
};
```

- [ ] **Step 2: 重写 `include/command/RemoveNode.hpp`**

```cpp
#pragma once

#include <QPointer>
#include <QVector>
#include "Command.h"
#include "PaintBoard.h"
#include "node/BaseNode.hpp"

class RemoveNodeCMD : public Command {
private:
    QPointer<BaseNode> m_node;
    PaintBoard* m_paint_board;
    QVector<Edge> m_snapshot;
public:
    ~RemoveNodeCMD() = default;
    RemoveNodeCMD(BaseNode* node, PaintBoard* paint_board)
        :m_node(node), m_paint_board(paint_board) {}

    bool execute() override {
        if(!m_node || !m_paint_board) return false;
        m_snapshot = m_paint_board->m_graph.edgesOf(m_node);
        if(!m_paint_board->m_graph.removeNode(m_node)) return false;
        m_node->setSelected(false);
        m_node->setVisible(false);
        return true;
    }
    void undo() override {
        if(!m_node || !m_paint_board) return;
        m_paint_board->m_graph.addNode(m_node);
        m_node->setVisible(true);
        for(const Edge& edge : m_snapshot)
            m_paint_board->m_graph.addEdge(edge.start_port, edge.stop_port);
    }
};
```

- [ ] **Step 3: 更新 `AddEdgeCMD` / `RemoveEdgeCMD` 以使用新的 DAGraph 契约**

`include/command/AddEdge.hpp` 的实现保持：
```cpp
    bool execute() override {
       return m_paint_board->m_graph.addEdge(m_port_src,m_port_dst);
    }
    void undo() override {
        m_paint_board->m_graph.removeEdge(m_port_src,m_port_dst);
    }
```
`include/command/RemoveEdge.hpp` 的实现保持：
```cpp
    bool execute() override {
        return m_paint_board->m_graph.removeEdge(m_port_src,m_port_dst);
    }
    void undo() override {
        m_paint_board->m_graph.addEdge(m_port_src,m_port_dst);
    }
```
> 两文件无需改动，仅确认与此契约一致。

- [ ] **Step 4: 运行命令测试**

Run:
```bash
cmake --build build -j4 --target test_cmdmanager && ctest --test-dir build -R test_cmdmanager --output-on-failure
```
Expected: 5 个用例全部 PASS（`addNodeUndoRedoKeepsObjectAlive`、`removeNodeRestoresEdges` 现在通过）。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add include/command/AddNode.hpp include/command/RemoveNode.hpp
git commit -m "fix: make node commands reversible without destroying nodes"
```

---

## Task 6: PaintBoard 临时端口与连线生命周期

**Files:**
- Modify: `include/PaintBoard.h`

- [ ] **Step 1: 重写 `include/PaintBoard.h`**

```cpp
#pragma once
#include <memory>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QQuickPaintedItem>
#include <QPainterPath>
#include "port/Port.hpp"
#include "utils/DAGraph.hpp"
#include "utils/Edge.hpp"


class PaintBoard : public QQuickPaintedItem {
    Q_OBJECT
    QML_ELEMENT
public:
    DAGraph m_graph;
    Edge m_drawing_edge;
    bool m_drawing_line = false;

    explicit PaintBoard(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {}

    void startDrawing(Port* start, QPointF pos) {
        if(!start) return;
        m_tmp_port = std::make_unique<Port>("__tmp__", PortType::Input,
                                            start->dataType(), pos, nullptr);
        m_drawing_edge = Edge(start, m_tmp_port.get());
        m_drawing_edge.midPoint = pos;
        m_drawing_line = true;
        update();
    }
    void moveDrawing(QPointF pos) {
        if(!m_drawing_line) return;
        if(m_drawing_edge.stop_port) m_drawing_edge.stop_port->setPosition(pos);
        m_drawing_edge.midPoint = pos;
        update();
    }
    void cancelDrawing() {
        m_drawing_line = false;
        m_drawing_edge = Edge{};
        m_tmp_port.reset();
        update();
    }
    void finishDrawing() { cancelDrawing(); }

    void paint(QPainter* painter) override {
        painter->setRenderHint(QPainter::Antialiasing, true);
        for(Edge& edge : m_graph.getAllEdges()){
            edge.calculateBezierPoint();
            edge.drawCurve(painter);
        }
        if(m_drawing_line){
            m_drawing_edge.drawCurve(painter);
        }
    }
private:
    std::unique_ptr<Port> m_tmp_port;
};
```

- [ ] **Step 2: 构建验证（NodeManager 仍引用旧接口，先修正其字段访问）**

Run:
```bash
cmake --build build -j4 2>&1 | tail -30
```
Expected: 若报错涉及 `m_drawing_edge.stop_port->deleteLater()` 或 `new Port`，这些将在 Task 7 移除；本 Task 内只需保证 `PaintBoard.h` 自身可编译。可先运行：
```bash
cmake --build build -j4 --target test_cmdmanager
```
Expected: `test_cmdmanager` 构建并测试通过（其包含 `PaintBoard.h`）。

- [ ] **Step 3: Commit（待用户授权）**

```bash
git add include/PaintBoard.h
git commit -m "fix: own temporary connection port in PaintBoard"
```

---

## Task 7: NodeManager 事件入口重构

**Files:**
- Modify: `include/NodeManager.h`
- Test: `tests/test_cmdmanager.cpp`（回归）

- [ ] **Step 1: 重写 `include/NodeManager.h`**

```cpp
#pragma once
#include <print>
#include <QObject>
#include <QPointer>
#include <QList>
#include <QPointF>
#include <QMap>
#include <QVector>
#include "PaintBoard.h"
#include "node/ImageLoad.hpp"
#include "utils/DAGraph.hpp"
#include "command/AddNode.hpp"
#include "command/AddEdge.hpp"
#include "command/RemoveEdge.hpp"
#include "command/RemoveNode.hpp"
#include "command/CmdManager.hpp"


class NodeManager : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
private:
    CmdManager m_cmd_manager;
    PaintBoard* m_paint_board = nullptr;
    NodeManager(QObject *parent = nullptr) : QObject(parent) {}

    void refresh() { if(m_paint_board) m_paint_board->update(); }

public:
    Q_INVOKABLE bool createNode(BaseNode *node){
        if(!node || !m_paint_board) return false;
        auto command = std::make_unique<AddNodeCMD>(node, m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        if(ok) std::println("创建节点成功");
        refresh();
        return ok;
    }
    Q_INVOKABLE bool removeNode(){
        if(!m_paint_board) return false;
        auto nodes = m_paint_board->m_graph.getSelectedNodes();
        if(nodes.isEmpty()) return false;
        for(auto* node : nodes){
            auto command = std::make_unique<RemoveNodeCMD>(node, m_paint_board);
            m_cmd_manager.executeCommand(std::move(command));
        }
        refresh();
        return true;
    }
    Q_INVOKABLE bool removeEdge(){
        if(!m_paint_board) return false;
        auto edges = m_paint_board->m_graph.getSelectedEdges();
        if(edges.isEmpty()) return false;
        for(const auto& edge : edges){
            auto command = std::make_unique<RemoveEdgeCMD>(edge.start_port, edge.stop_port, m_paint_board);
            m_cmd_manager.executeCommand(std::move(command));
        }
        refresh();
        return true;
    }
    Q_INVOKABLE bool undo() { bool ok = m_cmd_manager.undo(); refresh(); return ok; }
    Q_INVOKABLE bool redo() { bool ok = m_cmd_manager.redo(); refresh(); return ok; }

    Q_INVOKABLE void clickNodeEvent(QUuid node_uid) {
        if(!m_paint_board) return;
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            bool hit = (node->uuid() == node_uid);
            node->setZ(hit ? 1 : 0);
            node->setSelected(hit);
        }
        refresh();
    }
    Q_INVOKABLE void setPaintBoard(PaintBoard *board) { m_paint_board = board; }

    Q_INVOKABLE void nodeMoveEvent(QUuid node_uid, qreal dx, qreal dy){
        if(!m_paint_board) return;
        QPointF d_pos(dx, dy);
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() == node_uid){
                for(auto* port : node->getPorts())
                    port->movedeltaPos(d_pos);
                break;
            }
        }
        refresh();
    }
    Q_INVOKABLE void nodeResizeEvent(QUuid node_uid, qreal dw, qreal dh){
        if(!m_paint_board) return;
        for(auto* node : m_paint_board->m_graph.getAllNodes()){
            if(node->uuid() == node_uid){
                for(auto* port : node->getOutPorts())
                    port->movedeltaPos(QPointF(dw, 0));
                break;
            }
        }
        refresh();
    }
    Q_INVOKABLE void mouseMoveEvent(qreal x, qreal y){
        if(!m_paint_board) return;
        m_paint_board->moveDrawing(QPointF(x, y));
    }
    Q_INVOKABLE void mousePressEvent(const QPointF& pos, bool ctrl = false){
        if(!m_paint_board) return;
        for(Edge& edge : m_paint_board->m_graph.getAllEdges())
            edge.seleected = edge.isPointOnCurve(pos);
        for(BaseNode* node : m_paint_board->m_graph.getAllNodes()){
            if(node->boundingRect().contains(pos)){
                node->setSelected(ctrl ? !node->selected() : true);
            }else if(!ctrl){
                node->setSelected(false);
            }
        }
        refresh();
    }
    Q_INVOKABLE void setOutputPort(Port* port, qreal x, qreal y){
        if(!m_paint_board || !port) return;
        port->setPosition(QPointF(x, y));
        if(!m_paint_board->m_drawing_line){
            m_paint_board->startDrawing(port, QPointF(x, y));
            refresh();
        }
    }
    Q_INVOKABLE void setInputPort(Port* port, qreal x, qreal y){
        if(!m_paint_board || !port) return;
        if(!m_paint_board->m_drawing_line) return;
        auto* src = m_paint_board->m_drawing_edge.start_port;
        if(!src){ m_paint_board->cancelDrawing(); refresh(); return; }
        if(port->isConnected()){
            m_paint_board->cancelDrawing();
            std::println("这个节点已经被连接");
            refresh();
            return;
        }
        if(port->father() == src->father()){
            m_paint_board->cancelDrawing();
            std::println("节点不能自己相连");
            refresh();
            return;
        }
        if(port->dataType() != src->dataType()){
            m_paint_board->cancelDrawing();
            std::println("两端数据类型不匹配");
            refresh();
            return;
        }
        port->setPosition(QPointF(x, y));
        auto command = std::make_unique<AddEdgeCMD>(src, port, m_paint_board);
        bool ok = m_cmd_manager.executeCommand(std::move(command));
        if(ok) std::println("连接成功");
        else   std::println("有回路或连线无效");
        m_paint_board->finishDrawing();
        refresh();
    }

public:
    NodeManager(const NodeManager&) = delete;
    NodeManager& operator=(const NodeManager&) = delete;
    static QObject* instance() {
        static NodeManager instance;
        return &instance;
    }
    ~NodeManager(){};
};
```

- [ ] **Step 2: 全量构建验证**

Run:
```bash
cmake --build build -j4
```
Expected: 全部目标构建成功（`main`、`test_port`、`test_dagraph`、`test_cmdmanager`）。

- [ ] **Step 3: 回归测试**

Run:
```bash
ctest --test-dir build --output-on-failure
```
Expected: 全部测试 PASS。

- [ ] **Step 4: Commit（待用户授权）**

```bash
git add include/NodeManager.h
git commit -m "fix: guard NodeManager state and refresh board on command changes"
```

---

## Task 8: 曲线命中、端口位置同步与 QML 集成

**Files:**
- Modify: `include/utils/Edge.hpp`
- Modify: `include/node/BaseNode.hpp`
- Modify: `include/node/ImageLoad.hpp`
- Modify: `qml/main.qml`
- Modify: `qml/node/ImageLoadNode.qml`
- Modify: `qml/node/ImageShowNode.qml`

- [ ] **Step 1: 在 `include/utils/Edge.hpp` 顶部补充头文件**

在 `#include <QPainterPath>` 之后加入：
```cpp
#include <QLineF>
#include <algorithm>
#include <limits>
```

- [ ] **Step 2: 用采样距离替换 `Edge::isPointOnCurve`**

在 `struct Edge` 内、`isPointOnCurve` 之前加入静态辅助函数，并替换 `isPointOnCurve` 实现：

```cpp
    static qreal distanceToSegment(const QPointF& p, const QPointF& a, const QPointF& b){
        QPointF ab = b - a;
        qreal len2 = ab.x()*ab.x() + ab.y()*ab.y();
        if(len2 <= 1e-9) return QLineF(p, a).length();
        qreal t = ((p.x()-a.x())*ab.x() + (p.y()-a.y())*ab.y()) / len2;
        t = std::clamp(t, 0.0, 1.0);
        QPointF proj = a + t * ab;
        return QLineF(p, proj).length();
    }
    bool isPointOnCurve(const QPointF& point) const {
        if(!start_port || !stop_port) return false;
        const QPointF P0 = start_port->position();
        const QPointF P1 = P0 + QPointF{200, 0};
        const QPointF P2 = stop_port->position() - QPointF{200, 0};
        const QPointF P3 = stop_port->position();
        const int samples = 24;
        qreal best = std::numeric_limits<qreal>::max();
        QPointF prev = P0;
        for(int i = 1; i <= samples; ++i){
            qreal t = static_cast<qreal>(i) / samples;
            qreal u = 1 - t;
            QPointF cur = u*u*u*P0 + 3*u*u*t*P1 + 3*u*t*t*P2 + t*t*t*P3;
            best = std::min(best, distanceToSegment(point, prev, cur));
            prev = cur;
        }
        return best < 8.0;
    }
```

- [ ] **Step 3: 在 `include/node/BaseNode.hpp` 增加端口位置同步接口**

在 `Q_INVOKABLE qreal getMinHeight(){return min_height;}` 之后加入：

```cpp
    Q_INVOKABLE void setInputPortPosition(int index, qreal x, qreal y){
        if(index >= 0 && index < m_input_ports.size())
            m_input_ports[index]->setPosition(QPointF(x, y));
    }
    Q_INVOKABLE void setOutputPortPosition(int index, qreal x, qreal y){
        if(index >= 0 && index < m_output_ports.size())
            m_output_ports[index]->setPosition(QPointF(x, y));
    }
```

- [ ] **Step 4: 修正 `include/node/ImageLoad.hpp` 的多余输入端口**

将构造函数体替换为：
```cpp
    ImageLoadNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "加载图片";
        m_output_ports.push_back(new Port("输出", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
```

- [ ] **Step 5: 修改 `qml/main.qml` 的画布鼠标事件与快捷键**

将 `MouseArea` 的 `onClicked` 替换为：
```qml
                onClicked: (event)=>{
                    var pos = Qt.point(event.x, event.y);
                    var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                    NodeManager.mousePressEvent(pos, ctrl)
                }
```

将 `Keys.onPressed` 替换为：
```qml
        Keys.onPressed: (event)=> {
            if (event.key == Qt.Key_Delete) {
                NodeManager.removeNode()
                NodeManager.removeEdge()
                event.accepted = true
            } else if (event.key == Qt.Key_Z && (event.modifiers & Qt.ControlModifier)) {
                NodeManager.undo()
                event.accepted = true
            } else if (event.key == Qt.Key_Y && (event.modifiers & Qt.ControlModifier)) {
                NodeManager.redo()
                event.accepted = true
            }
        }
```

- [ ] **Step 6: 在 `qml/node/ImageLoadNode.qml` 与 `ImageShowNode.qml` 中同步端口位置**

对**两个文件**做相同修改。在输入端口圆点 `Rectangle { ... }`（`color: "#17b9b9"` 的那个）内部、`MouseArea` 之后加入：
```qml
                            Component.onCompleted: {
                                var p = mapToItem(root.parent, width / 2, height / 2)
                                root.setInputPortPosition(index, p.x, p.y)
                            }
```

在输出端口圆点 `Rectangle { ... }`（`color: "#fe3521"` 的那个）内部、`MouseArea` 之后加入：
```qml
                            Component.onCompleted: {
                                var p = mapToItem(root.parent, width / 2, height / 2)
                                root.setOutputPortPosition(index, p.x, p.y)
                            }
```

- [ ] **Step 7: 全量构建并回归测试**

Run:
```bash
cmake --build build -j4 && ctest --test-dir build --output-on-failure
```
Expected: 构建成功，所有测试 PASS。

- [ ] **Step 8: 人工冒烟测试（需图形环境）**

Run:
```bash
./bin/main
```
Expected:
1. 添加「加载图片」「图片显示」节点。
2. 从加载图片的输出端口拖到图片显示节点的输入端口 → 生成连线。
3. 拖动节点 → 连线跟随；缩放节点右下角 → 输出端口跟随右边缘。
4. 点击连线中部 → 选中（变红）；Delete → 连线删除；端口可重新连线。
5. 删除节点 → 节点与其连线消失；Ctrl+Z → 节点与连线恢复；Ctrl+Y → 再次删除。
6. Ctrl+点击节点 → 多选；Delete → 多个节点一起删除。

- [ ] **Step 9: Commit（待用户授权）**

```bash
git add include/utils/Edge.hpp include/node/BaseNode.hpp include/node/ImageLoad.hpp qml/
git commit -m "feat: accurate edge picking, port position sync and multi-select"
```

---

## Task 9: 重写 README

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 用以下内容替换 `README.md`**

````markdown
# Ortdraw

基于节点的深度学习图像处理工具（原型）。使用 Qt 6 / QML 构建可拖拽的节点图编辑器，
底层集成 OpenCV，规划通过 ONNX Runtime 运行深度学习推理节点。

> 当前状态：**节点图编辑功能可用**（创建、连线、移动、缩放、删除、撤销/重做）；
> **执行引擎尚未实现**，节点暂不进行实际图像处理。

## 功能

- 节点图编辑：从侧栏添加节点，拖拽移动，右下角缩放。
- 端口连线：拖拽输出端口到输入端口建立有向连接，自动拒绝自连、类型不匹配、
  重复连接与成环。
- 连线渲染：三次贝塞尔曲线，点击曲线可选中。
- 删除与撤销：Delete 删除选中的节点/连线；`Ctrl+Z` 撤销，`Ctrl+Y` 重做。
- 多选：`Ctrl+点击` 节点可多选，批量删除。

## 目录结构

```
include/            头文件（大部分实现为 header-only）
  node/             节点基类与具体节点
  port/             端口
  utils/            DAGraph（邻接表 + 环检测）、Edge（贝塞尔曲线）
  command/          命令模式 undo/redo
  NodeManager.h     全局单例，QML 事件入口
  PaintBoard.h      画板（绘制连线）
src/main.cpp        程序入口
qml/                界面（主窗口、节点、按钮）
tests/              Qt Test 单元测试
assets/             图标与贴图
docs/               设计与实施文档
```

## 依赖

- CMake ≥ 3.30
- 支持 C++23 的编译器（GCC 13+ / Clang 16+ / MSVC 19.35+）
- Qt 6.8+（Core、Gui、Quick、Test）
- OpenCV 4/5

## 构建

```bash
cmake -S . -B build
cmake --build build -j4
```

可执行文件输出到 `bin/main`。

## 运行

```bash
./bin/main
```

## 测试

```bash
ctest --test-dir build --output-on-failure
```

测试使用 Qt Test，并通过 `QT_QPA_PLATFORM=offscreen` 在无显示环境运行。

## 已知限制

- 无执行引擎：节点不会真正处理图像。
- 删除的节点对象会保留在内存中直到画布销毁（撤销所需，编辑器规模下可忽略）。
- Windows 构建配置未在 CI 验证。

## 后续路线

1. 节点图执行引擎：拓扑排序 + 数据流求值。
2. ImageLoad / ImageShow 实际读写图像。
3. ONNX Runtime 推理节点。
4. 图的保存与加载。
````

- [ ] **Step 2: 验证无 Markdown 语法错误**

Run:
```bash
grep -c '^#' README.md
```
Expected: 输出大于 0（文件含标题），且上一步写入成功。

- [ ] **Step 3: Commit（待用户授权）**

```bash
git add README.md
git commit -m "docs: rewrite README for Ortdraw"
```

---

## 最终验证

- [ ] 清理并全新构建：

```bash
rm -rf build && cmake -S . -B build && cmake --build build -j4
```
Expected: 构建成功，无错误。

- [ ] 运行全部测试：

```bash
ctest --test-dir build --output-on-failure
```
Expected: `test_port`、`test_dagraph`、`test_cmdmanager` 三个测试可执行文件全部 PASS。

- [ ] 人工冒烟（Task 8 Step 8 的 6 项）。

---

## 自查记录

- **Spec 覆盖**：B1/B2/B3→Task 5；B4/B5/B6→Task 3；B7/B8/B9/B10→Task 7；M1/M2/M3→Task 6；M4→Task 8；U1/U2/U3→Task 7+8；C1/C2/C3/C4→Task 1；T1→Task 1/3/4；D1→Task 9。全部有对应任务。
- **占位符**：无 TBD/TODO；每个代码步骤给出完整代码。
- **类型一致性**：`Port::isConnected/setConnected`、`DAGraph::edgesOf`、`PaintBoard::startDrawing/moveDrawing/cancelDrawing/finishDrawing`、`CmdManager::canUndo/canRedo`、`BaseNode::setInputPortPosition/setOutputPortPosition` 在定义与使用处名称一致。
````
