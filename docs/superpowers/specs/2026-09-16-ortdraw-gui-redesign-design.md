# Ortdraw GUI 重设计 — 设计文档

- 日期：2026-09-16
- 状态：待评审
- 视觉原型：`docs/mockups/ortdraw-gui-v4.html`（可交互）、`v4-light.png`、`v4-grid.png`、`v3-menu-node.png`、`v3-collapse.png`
- 范围：重做 QML 界面（亮色默认 + 主题切换、节点库抽屉、属性面板、minimap、右键菜单），并补齐所需的少量 C++ 接口。
- 非目标：不实现节点图执行引擎；不新增真实图像处理节点（本 spec 只定义框架与现有两个节点的外观）。

## 1. 目标

把当前"窄侧栏 + 两个无文字图标按钮 + 裸节点矩形"的原型界面，升级为与 `ortdraw-gui-v4.html` 一致的编辑器界面：

1. 亮色为默认主题，支持亮/暗一键切换（后续可扩展第三套配色）。
2. 左侧节点库为**可折叠抽屉**，支持列表/网格两种视图、搜索、分类筛选、分组折叠。
3. 右侧属性面板，显示选中节点/连线信息，可折叠，窄屏自动收起。
4. 画布右下角 minimap，支持点击定位。
5. 右键上下文菜单（节点/连线/画布三种）。
6. 顶部菜单与工具栏、底部状态栏。
7. 保留并改进现有交互：拖拽移动、缩放节点、端口连线、选中、删除、撤销/重做、多选。

## 2. 视觉基准（来自原型）

| 区域 | 设计 |
| --- | --- |
| 配色 | 亮色默认（白面板/浅灰点阵画布/蓝色强调）；暗色为 Tokyo Night |
| 节点卡片 | 白底圆角卡，头部为分类色点 + 名称 + 类型名，正文左入右出端口 |
| 端口 | 12px 圆点，输入青、输出紫；旁带类型标签 pill（如 `图像 · Image`） |
| 连线 | 三次贝塞尔，未选中灰、选中蓝并发光 |
| 节点分类色 | 输入/输出 青、图像处理 蓝、数学/张量 橙、输出/显示 紫 |

节点几何常量（与现有后端一致）：卡片宽 `220`，头部高 `40`，端口行高 `28`，正文上下内边距 `6`；最小尺寸 `220×300`。

## 3. 布局与组件树

```
ApplicationWindow
├── TopBar (qml/chrome/TopBar.qml)
│   ├── Brand、菜单(文件/编辑/视图/运行/帮助)
│   ├── 撤销/重做、适应视图、网格、吸附、主题切换
│   └── 清空画布、运行(占位)
├── MainRow (SplitView 或 RowLayout + 动画)
│   ├── NodePalette (qml/palette/NodePalette.qml)          // 可折叠抽屉
│   │   ├── PaletteHeader (标题 + 列表/网格切换 + 折叠)
│   │   ├── SearchField (含清除按钮)
│   │   ├── CategoryFilterChips
│   │   └── PaletteList (list | grid) → PaletteItem
│   ├── CanvasArea (qml/canvas/CanvasArea.qml)
│   │   ├── GridBackground
│   │   ├── PaintBoard (现有，绘制连线)
│   │   ├── NodeLayer (BaseNode 实例)
│   │   ├── Minimap (qml/canvas/Minimap.qml)
│   │   └── RailToggle ×2 (折叠后出现)
│   └── Inspector (qml/inspector/Inspector.qml)             // 可折叠
├── StatusBar (qml/chrome/StatusBar.qml)
├── ContextMenu (qml/chrome/ContextMenu.qml)
└── Theme → C++ 单例
```

尺寸：节点库展开宽 `250`，属性面板展开宽 `286`，折叠宽 `0`；顶部栏高 `46`，状态栏高 `30`。

## 4. 主题系统

### 4.1 设计

新增 C++ 单例 `Theme`（`QML_ELEMENT` + `QML_SINGLETON`），集中所有颜色 token，供 QML 与绘制代码（`PaintBoard`、`BaseNode`）共同使用。切换主题只需改 `dark` 布尔值并发 `changed` 信号。

### 4.2 接口

```cpp
class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    // 颜色 token（QColor，全部 NOTIFY changed）
    Q_PROPERTY(QColor bg READ bg)              // 画布底
    Q_PROPERTY(QColor bgPanel READ bgPanel)    // 面板底
    Q_PROPERTY(QColor bgElev READ bgElev)      // 卡片/浮层底
    Q_PROPERTY(QColor bgHover READ bgHover)
    Q_PROPERTY(QColor border READ border)
    Q_PROPERTY(QColor borderSoft READ borderSoft)
    Q_PROPERTY(QColor fg READ fg)              // 主文本
    Q_PROPERTY(QColor fgDim READ fgDim)        // 次文本
    Q_PROPERTY(QColor fgBright READ fgBright)  // 强调文本
    Q_PROPERTY(QColor blue READ blue)          // 强调/选中
    Q_PROPERTY(QColor cyan READ cyan)
    Q_PROPERTY(QColor green READ green)
    Q_PROPERTY(QColor yellow READ yellow)
    Q_PROPERTY(QColor orange READ orange)
    Q_PROPERTY(QColor magenta READ magenta)
    Q_PROPERTY(QColor red READ red)
    Q_PROPERTY(QColor wire READ wire)          // 连线
    Q_PROPERTY(QColor grid READ grid)          // 网格
    Q_PROPERTY(QColor portIn READ portIn)
    Q_PROPERTY(QColor portOut READ portOut)
    Q_PROPERTY(QColor catInput READ catInput)
    Q_PROPERTY(QColor catProcess READ catProcess)
    Q_PROPERTY(QColor catMath READ catMath)
    Q_PROPERTY(QColor catOutput READ catOutput)
public:
    static QObject* instance();
    Q_INVOKABLE void toggle();
    Q_INVOKABLE void setDark(bool dark);
signals:
    void changed();
};
```

### 4.3 Token 取值

| token | 亮色 | 暗色 |
| --- | --- | --- |
| bg | `#eef0f7` | `#1a1b26` |
| bgPanel | `#ffffff` | `#16161e` |
| bgElev | `#ffffff` | `#1f2335` |
| bgHover | `#e9ebf6` | `#292e42` |
| border | `#cfd3e6` | `#292e42` |
| borderSoft | `#e4e6f2` | `#222436` |
| fg | `#4c5180` | `#a9b1d6` |
| fgDim | `#9095b8` | `#565f89` |
| fgBright | `#1f2335` | `#c0caf5` |
| blue | `#2e7de9` | `#7aa2f7` |
| green | `#587539` | `#9ece6a` |
| yellow | `#8c6c3e` | `#e0af68` |
| orange | `#b15c00` | `#ff9e64` |
| magenta | `#9854f1` | `#bb9af7` |
| red | `#f52a65` | `#f7768e` |
| cyan | `#007197` | `#7dcfff` |
| wire | `#b9bddb` | `#3b4261` |
| grid | `#d6d9e8` | `#292e42` |
| portIn = catInput | `#007197` | `#0db9d7` |
| portOut = catOutput | `#9854f1` | `#bb9af7` |
| catProcess | `#2e7de9` | `#7aa2f7` |
| catMath | `#b15c00` | `#ff9e64` |

- 默认 `dark = false`（亮色）。
- 切换即时生效：QML 绑定 `Theme.*` 自动刷新；`PaintBoard`/`BaseNode` 连接 `Theme::changed` 后 `update()`。
- 可选持久化：用 `QSettings` 记住上次主题（本 spec 标记为可选，默认不做）。

### 4.4 绘制代码改造

- `PaintBoard::drawCurve` 的画笔颜色来源：未选中用 `Theme::wire`，选中用 `Theme::blue`（不再硬编码 `Qt::green/red`）。
- `BaseNode::updatePaintNode` 背景色：未选中用 `Theme::bgElev`，选中用 `Theme::blue` 边框 + 浅色填充。节点 QML 改用 `Theme` 后，`BaseNode` 的 C++ 绘制可退化为仅画选中描边或完全交给 QML（见 5.1）。

## 5. 节点视觉

### 5.1 结构

新增可复用 QML 组件 `qml/node/NodeCard.qml`：
- 属性：`node`（`BaseNode*`）。
- 头部：分类色点（`Theme.cat*` 按 `node.category`）、`node.name`、`node.typeName`。
- 正文：用 `Repeater` 渲染 `node.inputPorts` / `node.outputPorts`，每行：端口圆点 + 名称 + 类型标签 pill。
- 状态：`node.selected` 为真时加蓝色描边与光晕。
- 右下角缩放手柄（沿用现有实现）。

`ImageLoadNode.qml` / `ImageShowNode.qml` 退化为薄封装：`ImageLoadNode { NodeCard { anchors.fill: parent; node: root } }`，消除当前两份近 200 行的重复。

**明确决策**：节点视觉全部由 `NodeCard.qml`（QML）负责；`BaseNode` 不再做 C++ 场景图绘制——去掉 `ItemHasContents` 标志与 `updatePaintNode` 实现，改为一个不含控件的透明 `QQuickItem` 容器。选中光晕、背景、边框均由 `NodeCard` 通过 `node.selected` 绑定实现，避免 `BaseNode` 与 QML 双重绘制。

### 5.2 后端新增

`BaseNode`：
```cpp
virtual QString typeName() const;   // ImageLoadNode→"ImageLoad", ImageShowNode→"ImageShow"
virtual QString category() const;   // "input" | "process" | "math" | "output"
Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
```
- `category()` 供节点卡强调色、节点库分组、minimap 着色共用。
- `description` 供属性面板与节点库展示；`name`/`description`/`color` 增加 `NOTIFY`（`nameChanged` 已存在）。

## 6. 节点库（NodePalette）

### 6.1 数据模型

新增 QML 单例 `qml/palette/NodeCatalog.qml`（`pragma Singleton`），提供节点类型清单：

```qml
// 每项：{ type, title, desc, cat, icon }
readonly property var items: [
  { type:"ImageLoad", title:"加载图片", desc:"从磁盘读取图像", cat:"input",   icon:"image" },
  { type:"ImageShow", title:"图片显示", desc:"预览处理结果",   cat:"output",  icon:"monitor" },
  { type:"Resize",    title:"缩放",     desc:"双线性插值调整尺寸", cat:"process", icon:"resize" },
  // ... 与原型一致
]
function componentUrl(type) // 返回 "qrc:/ImageLoadNode.qml" 等
```

> 目录中的类型若尚无对应 QML 实现，点击时置灰并提示"尚未实现"。

### 6.2 交互

- **搜索**：按 `title/desc/type` 过滤，非空时显示清除按钮。
- **分类筛选 chips**：`全部` + 各 `category`，单选过滤。
- **分组**：按 `category` 分组，组头含三角箭头 + 名称 + 数量徽章；点击折叠/展开，折叠状态保存在组件内。
- **视图切换**：`list`（行卡片，悬停出现 `＋`）与 `grid`（两列卡片，右上角 `＋`）。
- **添加节点**：点击条目或 `＋` → 用 `componentUrl` 创建 QML 对象（父项为画布节点层）→ `NodeManager.createNode(obj)`。
- **空结果**：显示"没有匹配的节点"。

## 7. 画布

- **网格**：点阵背景，随主题取 `Theme.grid`；工具栏"网格"按钮可切换显示。
- **平移/缩放**：拖拽空白平移，滚轮以光标为中心缩放，范围 `0.35–2.4`；工具栏"适应视图"。
- **选中**：点击节点选中（Ctrl 多选、Ctrl 点已选取消）；点击连线选中；点击空白清空。
- **连线**：输出端口拖到输入端口；拒绝自连、类型不符、重复、成环（沿用 `DAGraph` 规则）；进行中显示虚线预览。
- **Snap**：工具栏"吸附"开关，开启时拖动按 8px 网格对齐（原型行为）。

## 8. Minimap

- 位置：画布右下角，尺寸约 `210×140`，圆角卡片 + 主题背景。
- 内容：所有节点按 `category` 着色的矩形、连线（简化为直线）、当前视口蓝色矩形。
- 世界坐标 → minimap 坐标：对"节点包围盒 ∪ 当前视口"取并集，加 80px 边距，等比缩放居中。
- 交互：点击/拖动 minimap，将该点移动到视口中心。
- 刷新时机：图变化（`graphChanged`）与视图变化。

### 8.1 后端新增

`NodeManager`：
```cpp
Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphChanged)
Q_PROPERTY(int edgeCount READ edgeCount NOTIFY graphChanged)
Q_INVOKABLE QVariantList nodeSnapshots() const; // [{uuid,x,y,w,h,category,selected}]
Q_INVOKABLE QVariantList edgeSnapshots() const; // [{fromX,fromY,toX,toY,selected}]
signals: void graphChanged();
```
- 节点/边的世界坐标由端口位置给出：输出端口 `(x+w, y+...)`、输入端口 `(x, y+...)`，与 `PaintBoard` 的绘制一致。
- `graphChanged` 在 create/remove/undo/redo/move/resize 后发出；`move/resize` 用节流（≥30fps）避免 minimap 过载。

## 9. 右键菜单

新增 `qml/chrome/ContextMenu.qml`（基于 `Menu`/`MenuItem`，样式对齐主题）：

| 目标 | 菜单项 |
| --- | --- |
| 节点 | 复制节点、断开全部连接、置顶、删除节点（危险色） |
| 连线 | 删除连线、（反转方向，禁用占位） |
| 画布 | 添加节点 ▸（子菜单列出 `NodeCatalog`）、适应视图、显示网格、清空画布（危险色） |

`NodeManager` 新增动作：
```cpp
Q_INVOKABLE void disconnectNode(QUuid uid);
Q_INVOKABLE void bringToFront(QUuid uid);
Q_INVOKABLE void clearGraph();
```
- 复制：QML 侧按 `typeName()` 找到组件新建同类型节点，偏移 `(+48,+48)`，再 `NodeManager.createNode`；无需 C++ 复制端口，因此 C++ 不提供 `duplicateNode`。
- 命中判定：右键命中由 QML 元素树（`node`/连线 path/空白）决定；画布空白右键不改变选择。

## 10. 属性面板（Inspector）

- 未选中：占位提示。
- 选中节点：名称（可编辑，写回 `node.name`）、类型 + 分类、输入/输出端口 chips、位置 X/Y、分类色板（展示）。
- 选中连线：两端节点名、端口索引、"删除"提示。
- 可折叠：头部 `‹/›`；折叠后画布右侧出现悬浮 `‹` 手柄。
- 数据来源：`NodeManager` 增加
  ```cpp
  Q_PROPERTY(BaseNode* selectedNode READ selectedNode NOTIFY selectionChanged)
  Q_PROPERTY(QVariantMap selectedEdge READ selectedEdge NOTIFY selectionChanged)
  // selectedEdge: { from:uuid, fromPort:int, to:uuid, toPort:int }，无选中时为空 map
  Q_PROPERTY(bool hasNodeSelection READ hasNodeSelection NOTIFY selectionChanged)
  Q_PROPERTY(bool hasEdgeSelection READ hasEdgeSelection NOTIFY selectionChanged)
  signals: void selectionChanged();
  ```
  `clickNodeEvent`/`mousePressEvent`/连线选中后更新并 `emit selectionChanged()`。

## 11. 顶部栏与状态栏

- 顶部栏：品牌、菜单（本 spec 仅"视图/运行/帮助"等可点占位）、撤销/重做、适应视图、网格、吸附、主题切换、清空画布、运行（禁用占位，提示"执行引擎尚未实现"）。
- 状态栏：就绪状态、节点数、连线数、当前主题、选中对象、缩放百分比与 `−/+`。

## 12. 折叠行为

- 左右面板宽度带 `200ms` 过渡；折叠到 0 时在画布对应边缘显示悬浮手柄（`›`/`‹`）用于展开。
- 自动折叠：窗口宽 `< 1180px` 收起属性面板；`< 960px` 再收起节点库；由 `ApplicationWindow.width` 的 `onWidthChanged` 驱动。

## 13. 快捷键

| 键 | 行为 |
| --- | --- |
| `Delete` | 删除选中节点/连线 |
| `Ctrl+Z` / `Ctrl+Y` | 撤销 / 重做 |
| `Ctrl+D` | 复制选中节点 |
| `Ctrl+0` | 适应视图 |
| `Ctrl+点击` | 多选节点 |
| `Esc` | 关闭右键菜单/取消连线 |

## 14. 文件清单

**新增**
- `include/Theme.h`（C++ 单例，header-only 可选）
- `qml/node/NodeCard.qml`
- `qml/palette/NodePalette.qml`
- `qml/palette/NodeCatalog.qml`（pragma Singleton）
- `qml/chrome/TopBar.qml`、`qml/chrome/StatusBar.qml`、`qml/chrome/ContextMenu.qml`
- `qml/canvas/CanvasArea.qml`、`qml/canvas/Minimap.qml`
- `qml/inspector/Inspector.qml`

**修改**
- `qml/main.qml`（改为组合上述组件）
- `qml/node/ImageLoadNode.qml`、`ImageShowNode.qml`（改为 `NodeCard` 薄封装）
- `include/node/BaseNode.hpp`（`typeName/category/description` + 信号）
- `include/node/ImageLoad.hpp`、`ImageShow.hpp`（覆写 `typeName/category`）
- `include/PaintBoard.h`（颜色取自 `Theme`）
- `include/NodeManager.h`（`graphChanged`、`nodeCount/edgeCount`、`nodeSnapshots/edgeSnapshots`、`selectedNode/selectionChanged`、`disconnectNode/bringToFront/clearGraph`）
- `src/main.cpp`（注册 `Theme` 单例，若未用 `QML_ELEMENT` 自动注册）
- `assets.qrc`（加入新 QML）
- `CMakeLists.txt` / `tests/CMakeLists.txt`（新增头文件）

## 15. 边界与错误处理

- 节点库中未实现的类型：置灰 + tooltip。
- Minimap 空图：显示空背景，不报错。
- 折叠/展开时画布尺寸变化：重新计算视口与 minimap。
- 主题切换在绘制线程：仅改颜色并 `update()`，不做场景图重建。
- 右键菜单越界：按窗口边界翻转，避免超出屏幕。

## 16. 验证方式

- 构建：`cmake -S . -B build && cmake --build build -j4` 通过。
- 单元测试：现有 5 个测试套件保持通过；新增对 `Theme`（token 完整、切换发信号）与 `NodeManager::nodeSnapshots/edgeSnapshots` 的测试。
- QML 语法：`qmllint` 对新增 QML 无语法错误。
- 手工冒烟（需图形环境）：主题切换、抽屉折叠与自动收起、列表/网格切换、搜索与筛选、minimap 定位、三种右键菜单、连线/移动/缩放/删除/撤销重做。

## 17. 实施阶段建议

1. **Theme**：C++ 单例 + PaintBoard/BaseNode 取色 + 主题切换按钮。
2. **节点卡与画布布局**：`NodeCard`、`ImageLoadNode/ImageShowNode` 薄封装、`CanvasArea` 网格。
3. **节点库**：`NodeCatalog` + `NodePalette`（列表/网格/搜索/筛选/分组）。
4. **折叠与状态栏/顶部栏**。
5. **Minimap**：NodeManager 快照接口 + `Minimap`。
6. **右键菜单与 Inspector**：NodeManager 选择/动作接口 + `ContextMenu` + `Inspector`。
7. **测试与文档**：补测试、更新 README 截图与说明。

每阶段结束都应可构建、可运行、测试通过。

## 18. 风险与取舍

- 主题 token 数量较多，需保证 QML 与 C++ 命名一致；用单元测试锁定 token 齐全。
- 双重绘制风险已通过 5.1 的决策消除：`BaseNode` 不再做 C++ 绘制，视觉全部在 `NodeCard.qml`。实现时需确认现有测试与 `NodeManager` 未依赖 `BaseNode::updatePaintNode` 的副作用（如 `m_bg_color` 的自适应修改）。
- Minimap 高频移动刷新可能影响性能：采用节流。
- 新 QML 组件较多，`assets.qrc` 与目录结构需同步维护。
