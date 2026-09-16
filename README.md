# Ortdraw

基于节点的深度学习图像处理工具（原型）。使用 Qt 6 / QML 构建可拖拽的节点图编辑器，
底层集成 OpenCV，规划通过 ONNX Runtime 运行深度学习推理节点。

> 当前状态：**节点图编辑与界面重设计可用**（创建、连线、移动、缩放、删除、撤销/重做、
> 主题切换、属性编辑、minimap）；**执行引擎尚未实现**，节点暂不进行实际图像处理。

## 界面

- **主题**：默认亮色主题，工具栏 `☾ / ☀` 按钮切换亮/暗，全界面即时生效。
- **节点库抽屉**：列表 / 网格视图切换、搜索、分类筛选、分组折叠，点击条目或 `＋` 添加节点。
- **可折叠面板**：节点库与属性面板均可折叠；窄屏自动收起（宽度 `< 1180` 收起属性，`< 960` 收起节点库）。
- **画布**：点阵网格、平移 / 缩放，`适应视图`（`Ctrl+0`）。
- **Minimap**：右下角缩略图，点击可定位视图。
- **右键上下文菜单**：针对节点 / 连线 / 画布提供不同操作（复制、断开、置顶、删除、添加节点、适应视图、清空画布）。
- **属性面板**：显示所选节点的名称（可编辑）、类型、输入/输出端口、位置与描述，并可删除节点或连线。

## 功能

- 节点图编辑：从侧栏添加节点，拖拽移动，右下角缩放。
- 端口连线：先点击输出端口，再点击输入端口即可建立有向连接，连接过程中显示跟随指针的预览线；
  自动拒绝自连、类型不匹配、重复连接与成环。
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
  Theme.h           Theme 单例（颜色/亮暗主题）
  NodeManager.h     全局单例，QML 事件入口
  PaintBoard.h      画板（绘制连线）
src/main.cpp        程序入口
qml/                界面（主窗口与各组件）
  palette/          节点库（NodeCatalog / NodePalette / PaletteItem）
  canvas/           CanvasArea、Minimap
  chrome/           TopBar、StatusBar、ContextMenu
  inspector/        Inspector（属性面板）
  node/NodeCard.qml 节点卡片
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

开发/验证：`./bin/main --demo` 会额外创建两个示例节点（ImageLoad、ImageShow），
便于在无鼠标环境下截图核对界面。

## 测试

```bash
ctest --test-dir build --output-on-failure
```

测试使用 Qt Test，并通过 `QT_QPA_PLATFORM=offscreen` 在无显示环境运行。

## 已知限制

- 已实现 6 种节点，均未执行实际运算；参数在节点内编辑：
  - 加载图片 ImageLoad：输出 图像(Image)
  - 图片显示 ImageShow：输入 图像(Image)
  - 缩放 Resize：输入 图像(Image)；输出 图像(Image)；节点内显示输入尺寸，可按尺寸(宽/高)或百分比设定输出
  - 高斯模糊 Blur：输入 图像(Image)；输出 图像(Image)；节点内填核大小
  - 阈值二值化 Threshold：输入 图像(Image)；输出 图像(Image)；节点内拖动条设定阈值(0-255)
  - 卷积 Conv：输入 图像(Image)、卷积核(Tensor)；输出 图像(Image)
- 无执行引擎：节点不会真正处理图像。
- 删除的节点对象会保留在内存中直到画布销毁（撤销所需，编辑器规模下可忽略）。
- Windows 构建配置未验证。

## 后续路线

1. 节点图执行引擎：拓扑排序 + 数据流求值。
2. ImageLoad / ImageShow 实际读写图像。
3. ONNX Runtime 推理节点。
4. 图的保存与加载。
