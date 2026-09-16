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
