# Ortdraw

基于节点的深度学习图像处理工具（原型）。使用 Qt 6 / QML 构建可拖拽的节点图编辑器，
底层集成 OpenCV，规划通过 ONNX Runtime 运行深度学习推理节点。

> 当前状态：**节点图编辑与执行引擎可用**（创建、连线、移动、缩放、删除、撤销/重做、
> 主题切换、属性编辑、minimap）；内置图像节点通过 OpenCV 进行实际运算，结果缓存并预览；
> 可扩展的节点执行 SDK 已就绪。

## 界面

- **主题**：默认亮色主题，工具栏 `☾ / ☀` 按钮切换亮/暗，全界面即时生效。
- **节点库抽屉**：列表 / 网格视图切换、搜索、分类筛选、分组折叠，点击条目或 `＋` 添加节点。
- **可折叠面板**：节点库与属性面板均可折叠；窄屏自动收起（宽度 `< 1180` 收起属性，`< 960` 收起节点库）。
- **画布**：背景可切换为 空白 / 点阵 / 网格（工具栏 ▦ 循环切换），平移 / 缩放，`运行 / 停止（F5）、适应视图`（`Ctrl+0`）。
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

## 执行引擎

- **运行**：顶栏右上角的「运行 / 停止」按钮触发整图求值。求值在**后台线程**按拓扑序进行，
  不阻塞界面；运行中按钮变为「停止」，点击可取消。状态栏左侧实时显示引擎状态
  （`就绪` / `运行中…` / `完成` / `失败` / `已取消`）。
- **实际运算**：`ImageLoad` 节点内的「选择图片」按钮通过**系统原生文件对话框**选图并读取为图像；
  `Resize`（线性缩放）、`Blur`（高斯模糊）、`Threshold`（灰度二值化）、`Conv`（`cv::filter2D` 卷积）
  均调用 **OpenCV** 实际运算。
- **真实预览**：`ImageShow` 与各节点的缩略图显示该节点最近一次执行的**真实结果**；
  尚未产生结果时显示占位图。
- **错误处理**：节点执行失败时，卡片右上角显示红点，悬停红点显示错误文本；状态栏变为「失败」。
  单个节点失败不会中断其余可求值的分支（其上/下游按依赖跳过）。
- **拓扑求值**：引擎对图做快照后在线程内用 Kahn 算法做拓扑排序，解析每个节点的输入
  （沿入边取上游缓存输出），逐节点调用执行器；结果图像经队列信号回到主线程写入缓存，
  QML 通过 `image://nodeimage/<uuid>` 读取。
- **SDK**：节点执行抽象为 `NodeExecutor`（执行接口）与 `NodeRegistry`（类型 → 执行器注册表），
  内置执行器在启动时注册；`GraphExecutor` 负责拓扑、后台求值与错误传播；`ImageStore`
  （`QQuickImageProvider`）缓存结果图像。

### 端口类型

端口数据类型收敛为 `Image / Tensor / Number / Bool / Any`（原 `Float` 等数值类型统一为 `Number`；
`Any` 可与任意类型连接）。张量端口使用 **Tensorvia** 的 `via::Tensor`。

### 当前限制

- **手动运行**：修改参数或连线后需重新点击「运行」，尚无自动重算。
- `Conv` 需要**张量（Tensor）类型的卷积核输入**；当前尚无产生张量的节点，需先提供卷积核数据。
- **ONNX 推理节点尚未实现**，留待后续基于本执行 SDK 添加。

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
  engine/           执行引擎 SDK（NodeData / NodeExecutor / NodeRegistry / GraphExecutor / ImageStore）
    executors/      内置执行器（ImageLoad / ImageShow / Resize / Blur / Threshold / Conv）
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
- **Tensorvia**（张量库；CMake 目标 `Tensorvia::tensorvia`，头文件 `<tensorvia/core/tensor.h>`）

## 开发辅助

`./bin/main --demo` 会创建两个示例节点（加载图片、图片显示），便于无鼠标环境截图/验证。

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

- 已实现 6 种节点，均通过执行引擎实际运算；参数在节点内编辑：
  - 加载图片 ImageLoad：输出 图像(Image)
  - 图片显示 ImageShow：输入 图像(Image)
  - 缩放 Resize：输入 图像(Image)；输出 图像(Image)；节点内显示输入尺寸，可按尺寸(宽/高)或百分比设定输出
  - 高斯模糊 Blur：输入 图像(Image)；输出 图像(Image)；节点内填核大小
  - 阈值二值化 Threshold：输入 图像(Image)；输出 图像(Image)；节点内拖动条设定阈值(0-255)
  - 卷积 Conv：输入 图像(Image)、卷积核(Tensor)；输出 图像(Image)
- 需手动点击「运行」触发求值；改动参数或连线后需重新运行（详见「执行引擎 → 当前限制」）。
- 删除的节点对象会保留在内存中直到画布销毁（撤销所需，编辑器规模下可忽略）。
- Windows 构建配置未验证。

## 后续路线

1. 参数 / 连线变化后的**自动重算**（当前为手动运行）。
2. 更多节点类型：张量生成与运算节点、张量结果预览。
3. **ONNX Runtime 推理节点**（基于现有执行 SDK 扩展）。
4. 异步图像加载与更大的图执行性能优化。
