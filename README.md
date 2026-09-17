# Ortdraw

基于节点的深度学习图像处理工具（原型）。使用 Qt 6 / QML 构建可拖拽的节点图编辑器，
底层集成 OpenCV，规划通过 ONNX Runtime 运行深度学习推理节点。

> 当前状态：**节点图编辑与界面重设计可用**（创建、连线、移动、缩放、删除、撤销/重做、
> 主题切换、属性编辑、minimap）；**执行引擎尚未实现**，节点暂不进行实际图像处理。

## 界面

- **主题**：默认亮色主题，工具栏 `☾ / ☀` 按钮切换亮/暗，全界面即时生效。
- **节点库抽屉**：列表 / 网格视图切换、搜索、分类筛选、分组折叠，点击条目或 `＋` 添加节点。
- **可折叠面板**：节点库与属性面板均可折叠；窄屏自动收起（宽度 `< 1180` 收起属性，`< 960` 收起节点库）。
- **画布**：背景可切换为 空白 / 点阵 / 网格（工具栏 ▦ 循环切换），平移 / 缩放，`适应视图`（`Ctrl+0`）。
- **Minimap**：右下角缩略图，点击可定位视图。
- **右键上下文菜单**：针对节点 / 连线 / 画布提供不同操作（复制、断开、置顶、删除、添加节点、适应视图、清空画布）。
- **属性面板**：显示所选节点的名称（可编辑）、类型、输入/输出端口、位置与描述，并可删除节点或连线。

## 功能

- 节点图编辑：从侧栏添加节点，拖拽移动，右下角缩放。
- 端口连线：从输出端口按住拖拽到输入端口释放即可连线（也可从输入端口反向拖到输出端口）；
  可在设置中切回点击方式。连接过程中显示跟随指针的预览线；
  自动拒绝自连、类型不匹配、重复连接与成环。
- 连线渲染：支持 Spline/Linear/Straight 三种渲染模式（默认 Spline，可在设置中切换），点击连线可选中。
- 删除与撤销：Delete 删除选中的节点/连线；`Ctrl+Z` 撤销，`Ctrl+Y` 重做。
- 多选：`Ctrl+点击` 节点可多选，批量删除；多选/右键菜单等可在设置中开关。
- 图文件：新建 / 打开 / 保存（`Ctrl+N` / `Ctrl+O` / `Ctrl+S`，菜单「文件」）使用**系统原生文件对话框**
  （KDE/Dolphin 等桌面环境），文件格式为 `.ortdraw`（JSON），保存时自动补全扩展名。

## 设置

- **入口**：顶栏最右侧的齿轮按钮 `⚙`（标题「设置」）；或菜单「编辑 → 设置…」。
- **对话框**：全窗口浮层，分左侧分类与右侧设置行，支持按标签搜索。改动**即时生效**：
  每个控件直接读写 `Settings` 并立即作用于界面；「保存」只调用 `Settings.sync()` 落盘，
  「取消」/`Esc`/点击遮罩会回滚到打开对话框时的快照，底部另有「恢复默认」。
- **分类（8 个）**：外观、画布、节点、连线、交互、性能、快捷键、关于；
  其中「快捷键」「关于」为只读信息页。
- **持久化位置**：`QSettings`（IniFormat，UserScope，组织/应用名 `Ortdraw`/`Ortdraw`），
  Linux 下即 `~/.config/Ortdraw/Ortdraw.ini`；测试可用环境变量 `ORTDRAW_SETTINGS_PATH`
  指定 INI 文件以隔离真实配置。析构与「保存」时均会落盘。
- **已接入（修改即时生效）**：主题与强调色、画布背景（空白/点阵/网格）与吸附/间距、空格平移、适应视图边距、
  缩放范围、节点预览/缩略图高度/高度自适应/端口类型标签/文字渲染/圆角、
  连线渲染模式/线宽/中点显示/悬停高亮、Minimap 刷新率、抗锯齿、连线方式（拖拽/点击）、
  Ctrl 多选节点（关闭后 Ctrl+点击不再多选/切换选择）、启用右键菜单（关闭后右键不弹出上下文菜单）。
- **占位（界面标注「即将支持」，控件禁用）**：界面密度、语言、删除确认、
  自动断开旧连线、异步图像加载。

## 目录结构

```
include/            头文件（大部分实现为 header-only）
  node/             节点基类与具体节点
  port/             端口
  utils/            DAGraph（邻接表 + 环检测）、Edge（贝塞尔曲线）
  command/          命令模式 undo/redo
  Theme.h           Theme 单例（颜色/亮暗主题，读写 Settings）
  Settings.h        Settings 单例（QSettings 持久化偏好设置）
  NodeManager.h     全局单例，QML 事件入口
  PaintBoard.h      画板（绘制连线）
src/main.cpp        程序入口
qml/                界面（主窗口与各组件）
  palette/          节点库（NodeCatalog / NodePalette / PaletteItem）
  canvas/           CanvasArea、Minimap
  chrome/           TopBar、StatusBar、ContextMenu
  inspector/        Inspector（属性面板）
  settings/         SettingsDialog（设置对话框）
  node/NodeCard.qml 节点卡片
tests/              Qt Test 单元测试
assets/             图标与贴图
docs/               设计与实施文档
```

## 依赖

- CMake ≥ 3.30
- 支持 C++23 的编译器（GCC 13+ / Clang 16+ / MSVC 19.35+）
- Qt 6.8+（Core、Gui、Quick、Widgets、Test）
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
- 无执行引擎：节点不会真正处理图像；各图像节点当前显示的是**占位预览图**，接入执行后应替换为真实结果。
- 删除的节点对象会保留在内存中直到画布销毁（撤销所需，编辑器规模下可忽略）。
- Windows 构建配置未验证。

## 后续路线

1. 节点图执行引擎：拓扑排序 + 数据流求值。
2. ImageLoad / ImageShow 实际读写图像。
3. ONNX Runtime 推理节点。
4. 图的保存与加载。
