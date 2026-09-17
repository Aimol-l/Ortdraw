# Ortdraw 设置系统 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 `Settings` 单例（QSettings 持久化）与设置对话框，把已具备的功能接入设置项。

**Architecture:** C++ `Settings` 单例为唯一数据源；`Theme` 从其读取主题/强调色；`Edge`/`PaintBoard`/`NodeCard`/`CanvasArea`/`Minimap` 绑定各自设置；QML `SettingsDialog` 提供分类/搜索/控件，入口在 TopBar。

**Tech Stack:** C++23、Qt 6.11（Core/Gui/Quick/Controls/Test）、CMake、Qt Test。

**视觉基准:** `docs/mockups/ortdraw-settings-v1.html` 及截图 `set-light.png` / `set-dark.png` / `set-links.png`。
**Spec:** `docs/superpowers/specs/2026-09-17-ortdraw-settings-design.md`。

---

## 环境与约定
- 工作目录：`/home/aimol/Documents/C++/Workspace/Ortdraw`
- 构建：`cmake --build build -j4`；测试：`ctest --test-dir build --output-on-failure`；qmllint：`/usr/lib/qt6/bin/qmllint`
- 显示可用：`DISPLAY=:0 ./bin/main [--demo]`，截图 `DISPLAY=:0 import -window <WID>`（用 `xwininfo -root -tree | grep '"Ortdraw"'` 取 WID）
- 现有 7 个测试套件必须保持通过；不提交（除非用户要求）。

---

## Task 1: Settings 单例 + 主题接入

**Files:** Create `include/Settings.h`, `tests/test_settings.cpp`; Modify `include/Theme.h`, `src/main.cpp`, `tests/CMakeLists.txt`.

- [ ] **Step 1: 写失败测试 `tests/test_settings.cpp`**

```cpp
#include <QtTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include "Settings.h"

class TestSettings : public QObject {
    Q_OBJECT
private slots:
    void defaultsAreCorrect() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QCOMPARE(s.theme(), QString("light"));
        QVERIFY(!s.accentCustom());
        QVERIFY(s.showGrid());
        QCOMPARE(s.gridSpacing(), 26);
        QCOMPARE(s.renderMode(), QString("spline"));
        QCOMPARE(s.midpointMode(), QString("selected"));
        QCOMPARE(s.previewHeight(), 88);
        QCOMPARE(s.cornerRadius(), 10);
        QCOMPARE(s.minimapFps(), 30);
    }
    void writeEmitsAndClamps() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        QSignalSpy spy(&s, &Settings::gridSpacingChanged);
        s.setGridSpacing(999);
        QCOMPARE(s.gridSpacing(), 60);       // clamped
        QCOMPARE(spy.count(), 1);
        s.setCornerRadius(-5);
        QCOMPARE(s.cornerRadius(), 0);
        s.setRenderMode("linear");
        QCOMPARE(s.renderMode(), QString("linear"));
        s.setRenderMode("bogus");
        QCOMPARE(s.renderMode(), QString("spline"));  // invalid -> default
    }
    void persistsAndReloads() {
        QString path;
        { QTemporaryFile f; QVERIFY(f.open()); path = f.fileName(); f.close(); }
        { Settings s(path); s.setTheme("dark"); s.setAccentColor(QColor("#9854f1")); s.sync(); }
        { Settings s2(path); QCOMPARE(s2.theme(), QString("dark"));
          QVERIFY(s2.accentCustom()); QCOMPARE(s2.accentColor().name(), QString("#9854f1")); }
    }
    void resetDefaultsRestoresAndSignals() {
        QTemporaryFile f; QVERIFY(f.open());
        Settings s(f.fileName());
        s.setTheme("dark"); s.setShowGrid(false);
        QSignalSpy spy(&s, &Settings::themeChanged);
        s.resetDefaults();
        QCOMPARE(s.theme(), QString("light"));
        QVERIFY(s.showGrid());
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestSettings)
#include "test_settings.moc"
```

- [ ] **Step 2: 运行确认失败**：`cmake -S . -B build && cmake --build build -j4 --target test_settings 2>&1 | tail -20` → 找不到 `Settings.h`。

- [ ] **Step 3: 新建 `include/Settings.h`**

按 spec 第 2/3 节实现全部键（每项 `Q_PROPERTY` + NOTIFY，WRITE 做钳制/枚举校验后写 `QSettings` 并 emit）。要点：

```cpp
#pragma once
#include <QObject>
#include <QColor>
#include <QSettings>
#include <memory>

class Settings : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor WRITE setAccentColor NOTIFY accentColorChanged)
    Q_PROPERTY(bool accentCustom READ accentCustom NOTIFY accentColorChanged)
    Q_PROPERTY(bool showGrid READ showGrid WRITE setShowGrid NOTIFY showGridChanged)
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)
    Q_PROPERTY(int gridSpacing READ gridSpacing WRITE setGridSpacing NOTIFY gridSpacingChanged)
    Q_PROPERTY(bool spaceToPan READ spaceToPan WRITE setSpaceToPan NOTIFY spaceToPanChanged)
    Q_PROPERTY(int fitMargin READ fitMargin WRITE setFitMargin NOTIFY fitMarginChanged)
    Q_PROPERTY(qreal zoomMin READ zoomMin WRITE setZoomMin NOTIFY zoomRangeChanged)
    Q_PROPERTY(qreal zoomMax READ zoomMax WRITE setZoomMax NOTIFY zoomRangeChanged)
    Q_PROPERTY(bool showPreview READ showPreview WRITE setShowPreview NOTIFY showPreviewChanged)
    Q_PROPERTY(int previewHeight READ previewHeight WRITE setPreviewHeight NOTIFY previewHeightChanged)
    Q_PROPERTY(bool showPortTypeTags READ showPortTypeTags WRITE setShowPortTypeTags NOTIFY showPortTypeTagsChanged)
    Q_PROPERTY(bool autoHeight READ autoHeight WRITE setAutoHeight NOTIFY autoHeightChanged)
    Q_PROPERTY(QString textRender READ textRender WRITE setTextRender NOTIFY textRenderChanged)
    Q_PROPERTY(int cornerRadius READ cornerRadius WRITE setCornerRadius NOTIFY cornerRadiusChanged)
    Q_PROPERTY(QString renderMode READ renderMode WRITE setRenderMode NOTIFY renderModeChanged)
    Q_PROPERTY(int linkWidth READ linkWidth WRITE setLinkWidth NOTIFY linkWidthChanged)
    Q_PROPERTY(QString midpointMode READ midpointMode WRITE setMidpointMode NOTIFY midpointModeChanged)
    Q_PROPERTY(bool hoverHighlight READ hoverHighlight WRITE setHoverHighlight NOTIFY hoverHighlightChanged)
    Q_PROPERTY(bool ctrlMultiSelect READ ctrlMultiSelect WRITE setCtrlMultiSelect NOTIFY ctrlMultiSelectChanged)
    Q_PROPERTY(bool contextMenu READ contextMenu WRITE setContextMenu NOTIFY contextMenuChanged)
    Q_PROPERTY(int minimapFps READ minimapFps WRITE setMinimapFps NOTIFY minimapFpsChanged)
    Q_PROPERTY(bool antialias READ antialias WRITE setAntialias NOTIFY antialiasChanged)
public:
    explicit Settings(QObject* parent = nullptr);                 // 默认存储
    explicit Settings(const QString& iniPath, QObject* parent = nullptr); // 测试用
    ~Settings() override;
    static QObject* instance();
    static Settings* settings() { return static_cast<Settings*>(instance()); }

    // getters...
    // setters 示例：
    void setGridSpacing(int v) { v = qBound(10, v, 60); if (m_gridSpacing == v) return;
        m_gridSpacing = v; m_store->setValue("canvas/gridSpacing", v); emit gridSpacingChanged(); }
    void setRenderMode(const QString& v) {
        const QString n = (v == "linear" || v == "straight" || v == "spline") ? v : "spline";
        if (m_renderMode == n) return; m_renderMode = n;
        m_store->setValue("links/renderMode", n); emit renderModeChanged(); }
    // midpointMode: selected|hover|always|never；textRender: curve|native；theme: light|dark
    void setAccentColor(const QColor& c) { if (!c.isValid()) return;
        m_accentColor = c; m_accentCustom = true; m_store->setValue("appearance/accentColor", c.name());
        emit accentColorChanged(); }
    Q_INVOKABLE void resetDefaults();   // 写默认值并 emit 所有信号
    Q_INVOKABLE void sync() { m_store->sync(); }
signals: /* 每项的 *Changed，另加 themeChanged/accentColorChanged/zoomRangeChanged */
private:
    std::unique_ptr<QSettings> m_store;
    // 成员缓存全部设置值
    void load();
};
```

- [ ] **Step 4: 运行测试确认通过**：`cmake --build build -j4 --target test_settings && ctest --test-dir build -R test_settings --output-on-failure` → 4 用例 PASS。

- [ ] **Step 5: `Theme` 接入 Settings**（`include/Theme.h`）
- `Theme` 构造函数改为：`connect(Settings::settings(), &Settings::themeChanged, this, &Theme::changed); connect(... accentColorChanged ...)`。
- `bool dark() const { return Settings::settings()->theme() == "dark"; }`
- `void setDark(bool d){ Settings::settings()->setTheme(d ? "dark" : "light"); }`
- `blue()`：`auto s = Settings::settings(); return s->accentCustom() ? s->accentColor() : pick("#2e7de9","#7aa2f7");`
- 保留 `toggle()`。

- [ ] **Step 6: 注册单例**：`src/main.cpp` 加 `#include "Settings.h"` 与
  `qmlRegisterSingletonInstance("Settings", 1, 0, "Settings", Settings::instance());`
- `tests/CMakeLists.txt` 的 `TEST_HEADERS` 加 `Settings.h`。

- [ ] **Step 7: 全量构建与测试**：`cmake --build build -j4 && ctest --test-dir build --output-on-failure` → 8 套件通过。

---

## Task 2: 连线渲染模式 / 线宽 / 中点 / 悬停

**Files:** Modify `include/utils/Edge.hpp`, `include/PaintBoard.h`, `include/NodeManager.h`.

- [ ] **Step 1: `Edge` 支持三种模式**

```cpp
enum class LinkRenderMode { Spline, Linear, Straight };
static LinkRenderMode modeFrom(const QString& s);   // spline 默认

void drawCurve(QPainter*, const QColor& wire, const QColor& sel,
               LinkRenderMode mode, int width, bool selected) const;
bool isPointOnCurve(const QPointF& p, LinkRenderMode mode) const;
```
- spline：现状（0.25×距离）。
- linear：折线 出→(p0.x+15,p0.y)→(mid.x,p0.y)…（用 `QPainterPath` 加 `lineTo`+`quadTo` 圆角 6）：实现为
  `p0 -> (p0.x+L, p0.y) -> (p3.x-L, p3.y) -> p3`，其中 `L = max(15, |dx|*0.25)`，拐角用 `quadTo` 圆角。
- straight：`p0 -> p3`。
- `isPointOnCurve`：spline 采样 24 段；linear 对每段折线用 `distanceToSegment`；straight 直接 `distanceToSegment`；阈值 8。

- [ ] **Step 2: `PaintBoard` 读取设置**（`#include "Settings.h"`）
- `paint()`：`auto* st = Settings::settings(); painter->setRenderHint(QPainter::Antialiasing, st->antialias());`
  每条边 `edge.calculateBezierPoint(); edge.drawCurve(painter, wire, blue, mode, st->linkWidth(), edge.seleected);`
  中点显示：按 `midpointMode`（selected/always/never/hover）决定是否画中点（hover 用 `m_hovered` 标记）。
  悬停高亮：`hoverHighlight` 且该边为 hovered → 使用 selColor。
- 构造时 `connect(Settings::settings(), &Settings::renderModeChanged/linkWidthChanged/antialiasChanged/midpointModeChanged/hoverHighlightChanged, this, [this]{update();})`。
- 新增 `void setHoveredEdge(Port* from, Port* to)` 或 `int m_hovered_edge_index`；简化：`Q_INVOKABLE void setHoveredEdge(int index)`，index=-1 表示无。

- [ ] **Step 3: `NodeManager` 悬停命中**
- `mouseMoveEvent(x,y)` 中，若 `Settings::hoverHighlight()` 或 `midpointMode=="hover"`，遍历边做 `isPointOnCurve`，记录命中边下标并调用 `m_paint_board->setHoveredEdge(idx)`。

- [ ] **Step 4: 构建 + GUI 验证**（临时 demo 造 3 条边，切换 renderMode 截图三种样式）。
- [ ] **Step 5: 测试**：`test_edge` 增加 linear/straight 命中用例（端点/中点/远点）。

---

## Task 3: 节点视觉接入设置

**Files:** Modify `qml/node/NodeCard.qml`（及必要时 `qml/node/*.qml`）。

- [ ] 让 `NodeCard` 导入 `Settings` 并按设置绑定：
  - `previewBox.visible: card.previewSource != "" && Settings.showPreview`
  - `previewBox.height: Settings.previewHeight`
  - 端口类型 pill `visible: Settings.showPortTypeTags`
  - 圆角：`bg.radius: Settings.cornerRadius`（头部 `topLeftRadius/topRightRadius: Settings.cornerRadius-1`）
  - 文字：`renderType: Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering`（所有 Text）
  - 节点 QML 的高度绑定：`Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()`（6 个节点文件）
  - `contentHeight` 中预览高度用 `Settings.previewHeight`
- [ ] 构建 + qmllint + 临时 demo 截图验证（关预览/改高度/圆角/原生渲染）。
- [ ] 测试套件保持通过。

---

## Task 4: 画布与性能接入设置

**Files:** Modify `qml/canvas/CanvasArea.qml`, `qml/canvas/Minimap.qml`.

- [ ] `CanvasArea`：`import Settings`
  - 网格可见 `Settings.showGrid`；点阵间距 `Settings.gridSpacing`；`no-grid` 逻辑保留。
  - `spaceToPan`：为假时左键拖空白也可平移（`onPressed` 条件放宽）。
  - `applyZoom` 钳制用 `Settings.zoomMin/zoomMax`；`fitView` 用 `Settings.fitMargin`。
  - 初始 `UiBus.snapEnabled = Settings.snapToGrid`（`Component.onCompleted`），并把工具栏吸附开关与 `Settings.snapToGrid` 同步。
- [ ] `Minimap`：加节流——`Connections onGraphChanged` 里不直接 `requestPaint`，而是 `throttle.restart()`；`Timer { id: throttle; interval: Settings.minimapFps>0 ? 1000/Settings.minimapFps : 0; repeat:false; onTriggered: cv.requestPaint() }`；fps=0 时直接重绘。
- [ ] 构建 + qmllint + GUI 验证（关网格、改间距、关空格平移、改缩放范围、minimap 拖动流畅）。

---

## Task 5: 设置对话框与入口

**Files:** Create `qml/settings/SettingsDialog.qml`; Modify `qml/chrome/TopBar.qml`, `qml/UiBus.qml`, `qml/main.qml`, `assets.qrc`.

- [ ] `qml/UiBus.qml` 加 `signal settingsRequested()`。
- [ ] `qml/settings/SettingsDialog.qml`：按 `ortdraw-settings-v1.html` 实现（`Item` 覆盖式浮层，`visible` 控制，`z` 高）：
  - 头部：标题、搜索框、主题按钮、关闭；左侧导航（8 分类）；右侧 `Column` + 分组标题 + 设置行。
  - 每个设置行：标签/说明 + 控件（分段/开关/滑杆/下拉/色板/连线模式小预览）。
  - 搜索过滤：遍历行按 `data tag`（行文本）过滤，无结果显示空态。
  - 关闭/取消：回滚打开时的快照（`Component.onCompleted`/打开时记录 `Settings` 各值，取消时写回）；保存：`Settings.sync()` 后关闭。
  - 禁用项：`enabled:false` + 「即将支持」徽章（density/language/confirmDelete/connectMode/autoDisconnect/asyncImage）。
  - 连线模式小预览用 `Shape`/`Canvas` 或内联图片，选中高亮。
- [ ] `TopBar` 增加齿轮按钮（`⚙`）→ `UiBus.settingsRequested()`；菜单「编辑 → 设置…」同样触发。
- [ ] `main.qml` 实例化 `SettingsDialog { id: settingsDialog }` 并连接 `UiBus.settingsRequested` → 打开。
- [ ] `assets.qrc` 加入 `SettingsDialog.qml`（若拆分子组件一并加入）。
- [ ] 构建 + qmllint + GUI 验证：打开设置、切主题/强调色、切连线模式看到画布连线变化、开关生效、恢复默认、取消回滚。

---

## Task 6: 文档与整体校验

- [ ] `README.md`：补充「设置」章节（入口、分类、持久化位置、已实现/占位说明）。
- [ ] `rm -rf build && cmake -S . -B build && cmake --build build -j4`（无警告）+ `ctest --test-dir build --output-on-failure`（9 套件：含 test_settings）。
- [ ] `qmllint $(find qml -name '*.qml')` 无语法错误。
- [ ] GUI 冒烟（有显示）：改动设置 → 重启 → 设置保留；主题在启动即生效。

---

## 自查记录
- **Spec 覆盖**：Settings 模型/单例→Task 1；主题/强调色→Task 1；连线模式/线宽/中点/悬停→Task 2；节点设置→Task 3；画布/性能→Task 4；对话框/入口→Task 5；文档/校验→Task 6；禁用项在 Task 5 标注。
- **占位符**：C++/测试给出完整代码；QML 以 `ortdraw-settings-v1.html` 为逐条移植基准。
- **一致性**：`Settings` 键名、`Edge::LinkRenderMode`、`Settings::*Changed`、`UiBus.settingsRequested` 在各任务一致。
