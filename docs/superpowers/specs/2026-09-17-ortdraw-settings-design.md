# Ortdraw 设置系统 — 设计文档

- 日期：2026-09-17
- 状态：待评审
- 视觉原型：`docs/mockups/ortdraw-settings-v1.html`（截图 `set-light.png` / `set-dark.png` / `set-links.png`）
- 范围：实现一个模态设置对话框 + C++ `Settings` 单例（`QSettings` 持久化），并把**当前已具备的功能**接入设置项；暂未实现对应功能的项先显示为「即将支持」（禁用）。
- 非目标：不实现语言切换、图保存/加载、异步图像加载、拖拽连线等尚未存在的功能（这些项在对话框中占位、禁用）。

## 1. 目标

1. 新增 `Settings` 单例，集中管理偏好，持久化到磁盘，启动时加载、修改即生效。
2. 新增 QML 设置对话框，布局与交互对齐 `ortdraw-settings-v1.html`（左侧分类、顶部搜索、分组设置项、底部按钮）。
3. 把已有功能接入：主题、强调色、画布网格/吸附/缩放、节点预览/高度/端口标签/文字渲染/圆角、连线渲染模式/线宽/中点显示/悬停高亮、minimap 刷新上限、抗锯齿。
4. 未实现功能的设置项禁用并标注「即将支持」，避免出现无效控件。

## 2. 设置项模型

分类与键（默认值）：

| 分类 | 键 | 类型 | 默认 | 状态 |
| --- | --- | --- | --- | --- |
| 外观 | `appearance/theme` | string(`light`/`dark`) | `light` | 实现 |
| 外观 | `appearance/accentColor` | color | `#2e7de9` | 实现 |
| 外观 | `appearance/density` | string | `standard` | 占位（禁用） |
| 外观 | `appearance/language` | string | `zh_CN` | 占位（禁用） |
| 画布 | `canvas/showGrid` | bool | `true` | 实现 |
| 画布 | `canvas/snapToGrid` | bool | `false` | 实现 |
| 画布 | `canvas/gridSpacing` | int(10–60) | `26` | 实现 |
| 画布 | `canvas/spaceToPan` | bool | `true` | 实现 |
| 画布 | `canvas/fitMargin` | int(20–200) | `80` | 实现 |
| 画布 | `canvas/zoomMin` | real(0.1–1.0) | `0.35` | 实现 |
| 画布 | `canvas/zoomMax` | real(1.0–8.0) | `2.4` | 实现 |
| 节点 | `nodes/showPreview` | bool | `true` | 实现 |
| 节点 | `nodes/previewHeight` | int(60–160) | `88` | 实现 |
| 节点 | `nodes/showPortTypeTags` | bool | `true` | 实现 |
| 节点 | `nodes/autoHeight` | bool | `true` | 实现 |
| 节点 | `nodes/textRender` | string(`curve`/`native`) | `curve` | 实现 |
| 节点 | `nodes/cornerRadius` | int(0–20) | `10` | 实现 |
| 连线 | `links/renderMode` | string(`spline`/`linear`/`straight`) | `spline` | 实现 |
| 连线 | `links/width` | int(1–5) | `2` | 实现 |
| 连线 | `links/midpointMode` | string(`selected`/`hover`/`always`/`never`) | `selected` | 实现 |
| 连线 | `links/hoverHighlight` | bool | `true` | 实现 |
| 交互 | `interaction/ctrlMultiSelect` | bool | `true` | 实现 |
| 交互 | `interaction/contextMenu` | bool | `true` | 实现 |
| 交互 | `interaction/confirmDelete` | bool | `false` | 占位（禁用） |
| 交互 | `interaction/connectMode` | string(`click`/`drag`) | `click` | 占位（禁用） |
| 交互 | `interaction/autoDisconnect` | bool | `false` | 占位（禁用） |
| 性能 | `perf/minimapFps` | int(0/30/60) | `30` | 实现 |
| 性能 | `perf/antialias` | bool | `true` | 实现 |
| 性能 | `perf/asyncImage` | bool | `true` | 占位（禁用） |

> 快捷键页与关于页为只读信息，不进入持久化模型。

## 3. `Settings` 单例

```cpp
class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    // 每项一个 Q_PROPERTY + NOTIFY；WRITE 立即写入 QSettings 并 emit
    // 例：
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(int gridSpacing READ gridSpacing WRITE setGridSpacing NOTIFY gridSpacingChanged)
    ...
public:
    static QObject* instance();
    Q_INVOKABLE void resetDefaults();   // 恢复默认并 emit 全部信号
    Q_INVOKABLE void sync();            // 立即 flush 到磁盘
signals:
    void showGridChanged();
    ...
};
```

- 存储：`QSettings(QSettings::IniFormat, QSettings::UserScope, "Ortdraw", "Ortdraw")`，位置由 `QStandardPaths::AppConfigLocation` 决定。
- 写策略：WRITE 时立即 `setValue`；另提供 `sync()`；析构时 `sync()`。
- 单例注册沿用项目风格：`qmlRegisterSingletonInstance("Settings", 1, 0, "Settings", Settings::instance())`。
- 颜色以字符串（`#rrggbb`）存储，`Q_PROPERTY(QColor ...)`。

## 4. 主题接入

`Theme` 改为以 `Settings` 为数据源：

- `Theme.dark` 的 READ 取 `Settings.theme == "dark"`，WRITE 写 `Settings`；`Settings` 的对应信号触发 `Theme::changed`。
- `Theme` 增加 `accent`：`Theme.blue()` 返回 `Settings.accentColor`（默认 `#2e7de9`，暗色默认仍为 `#7aa2f7`——规则：若用户未改过强调色，则按当前主题用内置蓝；一旦用户设置，则亮暗都用该色）。
  - 简化实现：`Settings.accentColor` 默认空表示“未自定义”，`Theme.blue()` 在未自定义时返回主题内置蓝，否则返回用户色。设置页选中某个色即写入具体值。

## 5. 连线渲染模式

`Edge` 增加按模式绘制与命中：

- `spline`（现有）：控制点 = 0.25 × 欧氏距离（ComfyUI/LiteGraph）。
- `linear`：`出端口 → 水平 l=15 → 到中点竖直 → 水平 → 入端口`，圆角半径 6（对齐 LiteGraph LINEAR 的折线+圆角）。
- `straight`：两端点直接连线。
- `drawCurve(painter, wire, sel, mode, width)`，`isPointOnCurve(point, mode)` 按模式采样（linear/straight 采样折线段）。
- `PaintBoard` 从 `Settings` 读取 `renderMode`、`width`，并连接相关信号重绘。
- 中点显示：`selected`（默认）/`always`/`never`/`hover`。hover 由 `NodeManager.mouseMoveEvent` 用光标位置对边做命中，记录 `hovered_edge` 并刷新；`PaintBoard` 据此绘制中点与高亮。

## 6. 节点视觉接入

`NodeCard` 从 `Settings` 读取：

- `showPreview`：隐藏/显示预览框。
- `previewHeight`：预览框高度。
- `showPortTypeTags`：端口类型 pill 是否显示。
- `autoHeight`：为假时节点高度固定为 `BaseNode.getMinHeight()`（用户可手动拉伸）。
- `textRender`：`curve`/`native` → `Text.CurveRendering` / `Text.NativeRendering`。
- `cornerRadius`：卡片与头部圆角。

## 7. 画布与性能接入

`CanvasArea`：
- `showGrid` → 网格可见；`gridSpacing` → 点阵间距；`snapToGrid` → `UiBus.snapEnabled` 初值同步（吸附开关仍在工具栏）。
- `spaceToPan` → 是否要求按住空格才能拖动画布。
- `fitMargin` / `zoomMin` / `zoomMax` → `fitView()` 与缩放钳制。

`Minimap`：
- `perf/minimapFps` → 用一个计时器限制 `requestPaint()` 的频率（0=不限制）。
- 仅在需要时重绘（graphChanged/视图变化），并按 fps 上限节流。

`PaintBoard`：
- `perf/antialias` → `setRenderHint(QPainter::Antialiasing, ...)`。

## 8. 设置对话框（QML）

新增 `qml/settings/SettingsDialog.qml`（`Popup` 或覆盖式 `Item`）：

- 结构对齐原型：头部（标题 + 搜索 + 主题按钮 + 关闭）、左侧分类导航、右侧分组设置项、底部（恢复默认 / 取消 / 保存）。
- 控件：分段选择、开关、滑杆（含数值）、下拉、色板、连线模式小预览（内联 SVG 或 `Shape`）。
- 搜索：按标签文本过滤行；无结果显示空态。
- 每个设置行左侧标签/说明来自各分类定义；右侧控件双向绑定 `Settings.*`。
- 禁用项：显示「即将支持」徽章，控件 `enabled: false`。
- 取消：还原打开时的快照（打开时缓存 `Settings` 值，取消时写回）。
- 保存/立即生效：原型写「更改会立即生效」，因此保存按钮仅做 `Settings.sync()`；切换控件即时生效。「取消」则回滚。

入口：
- `TopBar` 增加齿轮按钮（`⚙`）→ `UiBus.openSettings()`。
- 菜单「编辑 → 设置…」同样打开。
- `Esc` 关闭。

## 9. 持久化与首次运行

- 首次运行无配置文件时使用默认值（亮色、网格开、Spline 等）。
- 恢复默认：`Settings.resetDefaults()` 写回默认并 emit 所有信号，界面立即刷新。
- 主题持久化：下次启动时 `Theme` 从 `Settings` 读 `theme`，避免闪烁（在加载 QML 前 `Settings` 构造完成）。

## 10. 测试与验证

- 单元测试 `tests/test_settings.cpp`：
  - 默认值正确（各项默认与上表一致）。
  - WRITE 后 READ 生效且发出对应 `*Changed` 信号。
  - `resetDefaults()` 后全部回到默认并发信号。
  - 越界值被钳制（如 gridSpacing、zoomMin/Max、width）。
  - 持久化：写入后用 `QSettings` 读回一致（测试用临时配置路径，避免污染真实配置——通过环境变量或构造参数指定 INI 文件）。
- 现有 7 个测试套件保持通过。
- `qmllint` 无语法错误。
- GUI 冒烟（有显示）：打开设置、切换主题/强调色、网格/吸附、连线三种模式、缩略图开关、恢复默认；重启后设置保留。

## 11. 文件清单

**新增**
- `include/Settings.h`
- `tests/test_settings.cpp`
- `qml/settings/SettingsDialog.qml`（可拆 `SettingsRow.qml`、`SettingsToggle.qml` 等小组件）

**修改**
- `include/Theme.h`（dark/accent 来自 Settings）
- `include/utils/Edge.hpp`（三种渲染模式 + 按模式命中）
- `include/PaintBoard.h`（线宽、抗锯齿、模式、悬停中点）
- `include/NodeManager.h`（`mouseMoveEvent` 记录 hovered edge）
- `qml/node/NodeCard.qml`（读取节点相关设置）
- `qml/canvas/CanvasArea.qml`、`qml/canvas/Minimap.qml`
- `qml/chrome/TopBar.qml`（设置入口）、`qml/UiBus.qml`（`openSettings` 信号）
- `qml/main.qml`（实例化 `SettingsDialog`）
- `src/main.cpp`（注册 `Settings`）、`assets.qrc`、`CMakeLists.txt`、`tests/CMakeLists.txt`
- `README.md`

## 12. 风险与取舍

- 设置项较多，逐项绑定易遗漏：用单元测试覆盖默认值/钳制/复位，QML 侧以 `Settings` 单一数据源绑定。
- hover 高亮需要每次鼠标移动做命中测试：边数量大时有开销，可加"仅当启用 hover 相关设置时"判断。
- `Theme` 与 `Settings` 互相依赖：明确 `Settings` 为数据源、`Theme` 为只读取色器，`Settings` 不依赖 `Theme`。
- 取消（回滚）需要快照，注意与"立即生效"的语义一致；spec 采用"打开时快照、取消回滚、保存仅 flush"。
