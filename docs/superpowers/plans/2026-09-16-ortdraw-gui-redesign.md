# Ortdraw GUI 重设计 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 按 `docs/superpowers/specs/2026-09-16-ortdraw-gui-redesign-design.md` 重做 QML 界面：亮色默认 + 主题切换、节点库抽屉、属性面板、minimap、右键菜单，并补齐所需 C++ 接口。

**Architecture:** 新增 C++ `Theme` 单例集中颜色 token（QML 与绘制代码共用）；节点视觉统一到可复用 `NodeCard.qml`，`BaseNode` 去掉 C++ 绘制；节点库/属性面板/画布/minimap/右键菜单拆为独立 QML 组件；`NodeManager` 暴露 `graphChanged`、节点/边快照、选择状态与少量动作接口。

**Tech Stack:** C++23、Qt 6.11（Core/Gui/Quick/QuickControls2/Test）、OpenCV 5、CMake 3.30、Qt Test + CTest、qmllint。

**视觉基准（权威来源）:** `docs/mockups/ortdraw-gui-v4.html`（可交互）+ 截图 `v4-light.png`、`v4-grid.png`、`v3-menu-node.png`、`v3-collapse.png`。所有颜色/间距/圆角/图标以该原型为准。

---

## 环境与约定

- 工作目录：`/home/aimol/Documents/C++/Workspace/Ortdraw`
- 构建：`rm -rf build && cmake -S . -B build && cmake --build build -j4`
- 测试：`ctest --test-dir build --output-on-failure`（无显示环境，测试用 `QT_QPA_PLATFORM=offscreen`）
- `qmllint`：`/usr/lib/qt6/bin/qmllint`（不在 PATH，用绝对路径）
- 该目录是 git 仓库，但**默认不提交**；仅在用户明确要求时 commit。
- 现有 5 个测试套件必须始终保持通过：`test_port`、`test_dagraph`、`test_edge`、`test_paintboard`、`test_cmdmanager`。
- QML 注册沿用现有风格：单例用 `qmlRegisterSingletonInstance("Theme",1,0,"Theme", Theme::instance())`，QML 侧 `import Theme`。

---

## 文件结构

**新增**
- `include/Theme.h` — 主题单例（header-only）
- `tests/test_theme.cpp`
- `qml/node/NodeCard.qml` — 可复用节点卡
- `qml/palette/NodeCatalog.qml` — 节点类型清单（`pragma Singleton`）
- `qml/palette/NodePalette.qml` — 节点库抽屉
- `qml/chrome/TopBar.qml`、`qml/chrome/StatusBar.qml`、`qml/chrome/ContextMenu.qml`
- `qml/canvas/CanvasArea.qml`、`qml/canvas/Minimap.qml`
- `qml/inspector/Inspector.qml`

**修改**
- `qml/main.qml` — 组合各组件
- `qml/node/ImageLoadNode.qml`、`qml/node/ImageShowNode.qml` — 改为 `NodeCard` 薄封装
- `include/utils/Edge.hpp` — `drawCurve` 增加颜色参数
- `include/PaintBoard.h` — 取色自 `Theme`，连接 `Theme::changed`
- `include/node/BaseNode.hpp` — 去掉 C++ 绘制；加 `typeName/category/description`
- `include/node/ImageLoad.hpp`、`ImageShow.hpp` — 覆写 `typeName/category`
- `include/NodeManager.h` — 快照/计数/选择/动作接口 + `graphChanged`
- `src/main.cpp` — 注册 `Theme`
- `assets.qrc` — 加入新 QML
- `CMakeLists.txt` / `tests/CMakeLists.txt` — 纳入 `Theme.h`
- `README.md` — 更新界面说明

---

## Task 1: Theme 单例

**Files:**
- Create: `include/Theme.h`
- Create: `tests/test_theme.cpp`
- Modify: `src/main.cpp`
- Modify: `CMakeLists.txt`, `tests/CMakeLists.txt`

- [ ] **Step 1: 写失败测试 `tests/test_theme.cpp`**

```cpp
#include <QtTest>
#include <QSignalSpy>
#include "Theme.h"

class TestTheme : public QObject {
    Q_OBJECT
private slots:
    void defaultsToLight() {
        Theme t;
        QVERIFY(!t.dark());
        QCOMPARE(t.bg().name(), QString("#eef0f7"));
        QCOMPARE(t.bgPanel().name(), QString("#ffffff"));
        QCOMPARE(t.catInput().name(), QString("#007197"));
    }
    void toggleFlipsAndEmits() {
        Theme t;
        QSignalSpy spy(&t, &Theme::changed);
        t.toggle();
        QVERIFY(t.dark());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(t.bg().name(), QString("#1a1b26"));
        t.setDark(true);            // 相同值不应再发信号
        QCOMPARE(spy.count(), 1);
        t.setDark(false);
        QCOMPARE(spy.count(), 2);
    }
    void allTokensValid() {
        Theme t;
        for(bool d : {false, true}) {
            t.setDark(d);
            for(const QColor& c : {t.bg(), t.bgPanel(), t.bgElev(), t.bgHover(), t.border(),
                t.borderSoft(), t.fg(), t.fgDim(), t.fgBright(), t.blue(), t.cyan(), t.green(),
                t.yellow(), t.orange(), t.magenta(), t.red(), t.wire(), t.grid(), t.portIn(),
                t.portOut(), t.catInput(), t.catProcess(), t.catMath(), t.catOutput()}) {
                QVERIFY2(c.isValid(), "theme token must be a valid color");
            }
        }
    }
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
```

- [ ] **Step 2: 运行确认失败**

Run: `cmake -S . -B build && cmake --build build -j4 --target test_theme 2>&1 | tail -20`
Expected: 编译失败 `Theme.h: No such file or directory`。

- [ ] **Step 3: 新建 `include/Theme.h`**

```cpp
#pragma once
#include <QObject>
#include <QColor>

class Theme : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool dark READ dark WRITE setDark NOTIFY changed)
    Q_PROPERTY(QColor bg READ bg NOTIFY changed)
    Q_PROPERTY(QColor bgPanel READ bgPanel NOTIFY changed)
    Q_PROPERTY(QColor bgElev READ bgElev NOTIFY changed)
    Q_PROPERTY(QColor bgHover READ bgHover NOTIFY changed)
    Q_PROPERTY(QColor border READ border NOTIFY changed)
    Q_PROPERTY(QColor borderSoft READ borderSoft NOTIFY changed)
    Q_PROPERTY(QColor fg READ fg NOTIFY changed)
    Q_PROPERTY(QColor fgDim READ fgDim NOTIFY changed)
    Q_PROPERTY(QColor fgBright READ fgBright NOTIFY changed)
    Q_PROPERTY(QColor blue READ blue NOTIFY changed)
    Q_PROPERTY(QColor cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QColor green READ green NOTIFY changed)
    Q_PROPERTY(QColor yellow READ yellow NOTIFY changed)
    Q_PROPERTY(QColor orange READ orange NOTIFY changed)
    Q_PROPERTY(QColor magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QColor red READ red NOTIFY changed)
    Q_PROPERTY(QColor wire READ wire NOTIFY changed)
    Q_PROPERTY(QColor grid READ grid NOTIFY changed)
    Q_PROPERTY(QColor portIn READ portIn NOTIFY changed)
    Q_PROPERTY(QColor portOut READ portOut NOTIFY changed)
    Q_PROPERTY(QColor catInput READ catInput NOTIFY changed)
    Q_PROPERTY(QColor catProcess READ catProcess NOTIFY changed)
    Q_PROPERTY(QColor catMath READ catMath NOTIFY changed)
    Q_PROPERTY(QColor catOutput READ catOutput NOTIFY changed)
public:
    explicit Theme(QObject* parent = nullptr) : QObject(parent) {}
    static QObject* instance() { static Theme t; return &t; }

    bool dark() const { return m_dark; }
    void setDark(bool d) { if(m_dark == d) return; m_dark = d; emit changed(); }
    Q_INVOKABLE void toggle() { setDark(!m_dark); }

    QColor bg()        const { return pick("#eef0f7", "#1a1b26"); }
    QColor bgPanel()   const { return pick("#ffffff", "#16161e"); }
    QColor bgElev()    const { return pick("#ffffff", "#1f2335"); }
    QColor bgHover()   const { return pick("#e9ebf6", "#292e42"); }
    QColor border()    const { return pick("#cfd3e6", "#292e42"); }
    QColor borderSoft()const { return pick("#e4e6f2", "#222436"); }
    QColor fg()        const { return pick("#4c5180", "#a9b1d6"); }
    QColor fgDim()     const { return pick("#9095b8", "#565f89"); }
    QColor fgBright()  const { return pick("#1f2335", "#c0caf5"); }
    QColor blue()      const { return pick("#2e7de9", "#7aa2f7"); }
    QColor cyan()      const { return pick("#007197", "#7dcfff"); }
    QColor green()     const { return pick("#587539", "#9ece6a"); }
    QColor yellow()    const { return pick("#8c6c3e", "#e0af68"); }
    QColor orange()    const { return pick("#b15c00", "#ff9e64"); }
    QColor magenta()   const { return pick("#9854f1", "#bb9af7"); }
    QColor red()       const { return pick("#f52a65", "#f7768e"); }
    QColor wire()      const { return pick("#b9bddb", "#3b4261"); }
    QColor grid()      const { return pick("#d6d9e8", "#292e42"); }
    QColor portIn()    const { return catInput(); }
    QColor portOut()   const { return catOutput(); }
    QColor catInput()  const { return pick("#007197", "#0db9d7"); }
    QColor catProcess()const { return pick("#2e7de9", "#7aa2f7"); }
    QColor catMath()   const { return pick("#b15c00", "#ff9e64"); }
    QColor catOutput() const { return pick("#9854f1", "#bb9af7"); }

signals:
    void changed();
private:
    QColor pick(const char* light, const char* dark) const {
        return QColor(m_dark ? dark : light);
    }
    bool m_dark = false;
};
```

- [ ] **Step 4: 运行测试确认通过**

Run: `cmake --build build -j4 --target test_theme && ctest --test-dir build -R test_theme --output-on-failure`
Expected: 3 个用例 PASS。

- [ ] **Step 5: 在 `src/main.cpp` 注册单例**

在 `qmlRegisterSingletonInstance("NodeManager", ...)` 之后加：
```cpp
    qmlRegisterSingletonInstance("Theme", 1, 0, "Theme", Theme::instance());
```
并在文件头加 `#include "Theme.h"`。

- [ ] **Step 6: 构建验证**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 全部（6 个）测试套件通过。

- [ ] **Step 7: Commit（待用户授权）**

```bash
git add include/Theme.h tests/test_theme.cpp src/main.cpp
git commit -m "feat: add Theme singleton with light/dark tokens"
```

---

## Task 2: 绘制代码接入主题，BaseNode 去掉 C++ 绘制

**Files:**
- Modify: `include/utils/Edge.hpp`
- Modify: `include/PaintBoard.h`
- Modify: `include/node/BaseNode.hpp`
- Test: `tests/test_paintboard.cpp`（回归）

- [ ] **Step 1: `Edge::drawCurve` 增加颜色参数**

把 `include/utils/Edge.hpp` 中的 `drawCurve` 改为：

```cpp
    void drawCurve(QPainter* painter, const QColor& wireColor, const QColor& selColor) const {
        QPainterPath path;
        QPen pen(seleected ? selColor : wireColor, 2);
        painter->setPen(pen);
        path.moveTo(start_port->position());
        QPointF controlPoint1 = start_port->position() + QPointF{200, 0};
        QPointF controlPoint2 = stop_port->position()  - QPointF{200, 0};
        path.cubicTo(controlPoint1, controlPoint2, stop_port->position());
        painter->drawPath(path);
        painter->save();
        painter->setBrush(seleected ? selColor : wireColor);
        painter->drawEllipse(midPoint, 4, 4);
        painter->restore();
    }
```
（`drawCurve` 变为 `const`；`midPoint` 仍由 `calculateBezierPoint()` 更新。）

- [ ] **Step 2: `PaintBoard` 使用 Theme 并响应主题变化**

在 `include/PaintBoard.h` 顶部加 `#include "Theme.h"`、`#include <QPointer>`。在构造函数中连接主题信号：

```cpp
    explicit PaintBoard(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
        connect(Theme::instance(), &Theme::changed, this, [this]{ update(); });
    }
```
把 `paint()` 改为：

```cpp
    void paint(QPainter* painter) override {
        auto* theme = static_cast<Theme*>(Theme::instance());
        painter->setRenderHint(QPainter::Antialiasing, true);
        for(Edge& edge : m_graph.getAllEdges()){
            edge.calculateBezierPoint();
            edge.drawCurve(painter, theme->wire(), theme->blue());
        }
        if(m_drawing_line){
            m_drawing_edge.drawCurve(painter, theme->wire(), theme->blue());
        }
    }
```
（`Theme::instance()` 返回 `QObject*`，故先 `static_cast`；若不方便，可在 `Theme` 增加 `static Theme* theme() { return static_cast<Theme*>(instance()); }` 并使用它——推荐后者，避免每帧转换。）

- [ ] **Step 3: `BaseNode` 移除 C++ 绘制**

在 `include/node/BaseNode.hpp`：
- 删除 `this->setFlag(QQuickItem::ItemHasContents, true);`（保留 `ItemIsFocusScope`）。
- 删除整个 `QSGNode* updatePaintNode(...) override { ... }` 方法。
- 删除不再需要的 `#include <QSGGeometry>`、`<QSGGeometryNode>`、`<QSGFlatColorMaterial>`、`<QSGSimpleRectNode>`。
- `setSelected` 保留 `this->update();`（QML 绑定会自行刷新，无副作用）。

- [ ] **Step 4: 构建与回归测试**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 构建成功，6 个测试套件全部通过。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add include/utils/Edge.hpp include/PaintBoard.h include/node/BaseNode.hpp
git commit -m "refactor: theme-aware painting and drop BaseNode C++ rendering"
```

---

## Task 3: NodeCard 节点卡与节点 QML 薄封装

**Files:**
- Create: `qml/node/NodeCard.qml`
- Modify: `qml/node/ImageLoadNode.qml`
- Modify: `qml/node/ImageShowNode.qml`
- Modify: `include/node/BaseNode.hpp`（`typeName/category/description`）
- Modify: `include/node/ImageLoad.hpp`、`ImageShow.hpp`
- Modify: `assets.qrc`

- [ ] **Step 1: `BaseNode` 增加类型元数据**

在 `include/node/BaseNode.hpp` 的 `public:` 区加入：

```cpp
    virtual QString typeName() const { return "Base"; }
    virtual QString category() const { return "process"; }
    Q_PROPERTY(QString description READ description WRITE setDescription NOTIFY descriptionChanged)
```
并加入 getter/setter 与信号：
```cpp
    QString description() const { return m_description; }
    void setDescription(const QString& d) { m_description = d; emit descriptionChanged(); }
signals:
    void descriptionChanged();
```

- [ ] **Step 2: 覆写节点元数据**

`include/node/ImageLoad.hpp` 的 `public:` 加：
```cpp
    QString typeName() const override { return "ImageLoad"; }
    QString category() const override { return "input"; }
```
`include/node/ImageShow.hpp` 加：
```cpp
    QString typeName() const override { return "ImageShow"; }
    QString category() const override { return "output"; }
```

- [ ] **Step 3: 新建 `qml/node/NodeCard.qml`**

按 `docs/mockups/ortdraw-gui-v4.html` 中 `.node` / `.node-head` / `.port-row` / `.port` 的视觉与尺寸实现（宽 220、头高 40、行高 28、正文内边距 6；卡片圆角 10、头圆角 9/9/0/0）。关键代码：

```qml
import QtQuick
import Theme
import NodeManager

Item {
    id: card
    property var node          // BaseNode*
    readonly property color accent: node && node.category === "input"  ? Theme.catInput
                                  : node && node.category === "math"   ? Theme.catMath
                                  : node && node.category === "output" ? Theme.catOutput
                                  : Theme.catProcess
    Rectangle {
        id: bg
        anchors.fill: parent
        radius: 10
        color: Theme.bgElev
        border.width: node && node.selected ? 2 : 1
        border.color: node && node.selected ? Theme.blue : Theme.border
        // 选中光晕
        layer.enabled: node && node.selected
        // 头部
        Rectangle {
            id: head
            width: parent.width; height: 40
            radius: 9
            color: "transparent"
            Rectangle { // 分类色点
                width: 8; height: 8; radius: 3; color: card.accent
                anchors.left: parent.left; anchors.leftMargin: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Text { anchors.left: parent.left; anchors.leftMargin: 27
                   anchors.verticalCenter: parent.verticalCenter
                   text: node ? node.name : ""; color: Theme.fgBright; font.pixelSize: 13; font.bold: true }
            Text { anchors.right: parent.right; anchors.rightMargin: 11
                   anchors.verticalCenter: parent.verticalCenter
                   text: node ? node.typeName : ""; color: Theme.fgDim; font.family: "monospace"; font.pixelSize: 10 }
            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.borderSoft }
        }
        // 端口行（输入左、输出右）
        Column {
            id: rows
            anchors.top: head.bottom; anchors.topMargin: 6
            anchors.left: parent.left; anchors.right: parent.right
            Repeater {
                model: node ? Math.max(node.inputPorts.length, node.outputPorts.length) : 0
                delegate: Item {
                    width: rows.width; height: 28
                    // 输入
                    Row { visible: node && index < node.inputPorts.length
                        anchors.left: parent.left; anchors.leftMargin: 20
                        height: parent.height; spacing: 6
                        Rectangle { width: 12; height: 12; radius: 6; color: Theme.portIn
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea { anchors.fill: parent; anchors.margins: -4
                                onClicked: NodeManager.setInputPort(modelData.self,
                                    mapToItem(card.parent, 6, 6).x, mapToItem(card.parent, 6, 6).y) }
                        }
                        Text { text: node.inputPorts[index].name; color: Theme.fg; font.pixelSize: 11
                               anchors.verticalCenter: parent.verticalCenter }
                        Rectangle { color: Theme.bg; border.color: Theme.borderSoft; radius: 4
                            Text { text: node.inputPorts[index].type === 0 ? "Image" : "?" } }
                    }
                    // 输出（右对齐，略——结构对称）
                }
            }
        }
    }
}
```

> 实现要点：端口圆点在输入侧位于卡片左缘 `x=-7`、输出侧 `x=width-7`（相对卡片坐标），`mapToItem(card.parent, ...)` 得到的坐标即画布坐标，与 `PaintBoard` 一致。端口 hover 放大到 1.45、发光。图标用内联 SVG（`Shape`/`Canvas` 或 `Image` + `source` 到 `qrc` SVG，见 Task 5 的图标集）。
>
> 由于完整 QML 较长，实现时以 `ortdraw-gui-v4.html` 的对应 DOM/CSS 为准逐条移植；`NodeCard` 必须暴露 `node` 属性且不持有所有权。

- [ ] **Step 4: 节点 QML 改薄封装**

`qml/node/ImageLoadNode.qml`：
```qml
import QtQuick
import ImageLoadNode

ImageLoadNode {
    id: root
    width: 220; height: 300
    property point dragStartPos: "0,0"
    NodeCard { anchors.fill: parent; node: root }
    // 标题栏拖动、右下角缩放手柄与连线逻辑，移植自 v4 原型
}
```
`ImageShowNode.qml` 同构（基类型换为 `ImageShowNode`）。

- [ ] **Step 5: `assets.qrc` 加入 `NodeCard.qml`**

在 `qml/node/ImageShowNode.qml` 行后加：
```xml
        <file alias="NodeCard.qml">qml/node/NodeCard.qml</file>
```

- [ ] **Step 6: 构建、qmllint、回归**

Run:
```bash
cmake -S . -B build && cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/node/NodeCard.qml qml/node/ImageLoadNode.qml qml/node/ImageShowNode.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功；qmllint 无语法错误；测试通过。

- [ ] **Step 7: Commit（待用户授权）**

```bash
git add qml/node include/node/BaseNode.hpp include/node/ImageLoad.hpp include/node/ImageShow.hpp assets.qrc
git commit -m "feat: reusable NodeCard and node metadata"
```

---

## Task 4: main.qml 重构为 TopBar / CanvasArea / StatusBar，主题切换与折叠

**Files:**
- Create: `qml/chrome/TopBar.qml`、`qml/chrome/StatusBar.qml`
- Create: `qml/canvas/CanvasArea.qml`
- Modify: `qml/main.qml`
- Modify: `assets.qrc`

- [ ] **Step 1: 新建 `qml/chrome/TopBar.qml`**

按 `v4-light.png` 顶栏实现：品牌、菜单按钮、撤销/重做、适应视图、网格、吸附、主题切换、清空画布、运行。对外信号：

```qml
import QtQuick
import QtQuick.Controls
import Theme

Rectangle {
    id: bar
    height: 46
    color: Theme.bgPanel
    signal undoRequested()
    signal redoRequested()
    signal fitRequested()
    signal gridToggled()
    signal clearRequested()
    // 主题切换按钮
    Button { text: Theme.dark ? "☀" : "☾"; onClicked: Theme.toggle() }
    // ... 其余控件按原型布局
}
```

- [ ] **Step 2: 新建 `qml/chrome/StatusBar.qml`**

```qml
import QtQuick
import Theme
import NodeManager

Rectangle {
    id: bar
    height: 30
    color: Theme.bgPanel
    property real zoom: 1.0
    Text { text: "节点 " + NodeManager.nodeCount + "  连线 " + NodeManager.edgeCount; color: Theme.fgDim }
    Text { text: "主题 " + (Theme.dark ? "暗色" : "亮色"); color: Theme.fgDim }
    Text { text: Math.round(bar.zoom * 100) + "%"; color: Theme.fg }
}
```

- [ ] **Step 3: 新建 `qml/canvas/CanvasArea.qml`**

承载网格背景、`PaintBoard`、节点层、minimap、折叠手柄，并处理平移/缩放。对外暴露 `paintBoard`、`nodeLayer`、`zoom`、`fitView()`。关键片段：

```qml
import QtQuick
import PaintBoard
import NodeManager
import Theme

Item {
    id: area
    property alias paintBoard: board
    property alias nodeLayer: nodes
    property real zoom: 1.0
    property real panX: 0
    property real panY: 0
    property bool gridVisible: true

    Rectangle { anchors.fill: parent; color: Theme.bg }
    // 点阵网格
    Canvas {
        anchors.fill: parent
        visible: area.gridVisible
        onPaint: { /* 按 26px 点阵，点的颜色取 Theme.grid */ }
        Connections { target: Theme; function onChanged(){ grid.requestPaint() } }
    }
    Item {
        id: world
        transform: [ Translate { x: area.panX; y: area.panY }, Scale { xScale: area.zoom; yScale: area.zoom } ]
        PaintBoard { id: board; width: 4000; height: 3000 }
        Item { id: nodes; width: 4000; height: 3000 }   // 节点层
    }
    MouseArea {
        anchors.fill: parent
        // 空白拖拽平移、滚轮缩放、点击取消选择、右键菜单
    }
    Minimap { anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 14
              world: world; zoom: area.zoom; panX: area.panX; panY: area.panY
              onJumpTo: (wx, wy) => { /* 让 (wx,wy) 居中 */ } }
    function fitView() { /* 计算节点包围盒，设置 zoom/pan */ }
    Component.onCompleted: NodeManager.setPaintBoard(board)
}
```

- [ ] **Step 4: `qml/main.qml` 改为组合**

```qml
import QtQuick
import QtQuick.Window
import Theme

ApplicationWindow {
    id: win
    width: 1600; height: 900
    visible: true
    minimumWidth: 900; minimumHeight: 600
    property bool leftCollapsed: width < 960
    property bool rightCollapsed: width < 1180
    color: Theme.bg

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        TopBar { Layout.fillWidth: true
            onUndoRequested: NodeManager.undo()
            onRedoRequested: NodeManager.redo()
            onFitRequested: canvas.fitView()
            onClearRequested: NodeManager.clearGraph() }
        RowLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 0
            NodePalette { Layout.preferredWidth: win.leftCollapsed ? 0 : 250
                          Layout.fillHeight: true; visible: !win.leftCollapsed }
            CanvasArea { id: canvas; Layout.fillWidth: true; Layout.fillHeight: true }
            Inspector { Layout.preferredWidth: win.rightCollapsed ? 0 : 286
                        Layout.fillHeight: true; visible: !win.rightCollapsed }
        }
        StatusBar { Layout.fillWidth: true; zoom: canvas.zoom }
    }
    // 折叠动画、快捷键（Delete/Ctrl+Z/Ctrl+Y/Ctrl+D/Ctrl+0/Esc）
}
```

- [ ] **Step 5: `assets.qrc` 加入新 QML**（TopBar、StatusBar、CanvasArea）

- [ ] **Step 6: 构建、qmllint、回归**

Run:
```bash
cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/main.qml qml/chrome/TopBar.qml qml/chrome/StatusBar.qml qml/canvas/CanvasArea.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功、qmllint 无语法错误、测试通过。

> 注：本 Task 完成后 `Minimap`、`NodePalette`、`Inspector` 尚未实现，可先创建空壳组件（`Item {}` 占位）以保证 main.qml 可加载；它们分别在 Task 5/7/9 填充。

- [ ] **Step 7: Commit（待用户授权）**

```bash
git add qml/main.qml qml/chrome qml/canvas assets.qrc
git commit -m "feat: split UI into TopBar, CanvasArea and StatusBar with theme and collapse"
```

---

## Task 5: 节点类型清单与节点库抽屉

**Files:**
- Create: `qml/palette/NodeCatalog.qml`
- Create: `qml/palette/NodePalette.qml`
- Create: `qml/palette/PaletteItem.qml`
- Modify: `assets.qrc`

- [ ] **Step 1: 新建 `qml/palette/NodeCatalog.qml`（pragma Singleton）**

```qml
pragma Singleton
import QtQuick

QtObject {
    readonly property var items: [
        { type:"ImageLoad", title:"加载图片", desc:"从磁盘读取图像",     cat:"input",   icon:"image"   },
        { type:"ImageShow", title:"图片显示", desc:"预览处理结果",       cat:"output",  icon:"monitor" },
        { type:"Resize",    title:"缩放",     desc:"双线性插值调整尺寸", cat:"process", icon:"resize"  },
        { type:"Blur",      title:"高斯模糊", desc:"可调核大小的模糊",   cat:"process", icon:"blur"    },
        { type:"Threshold", title:"阈值二值化", desc:"固定 / 自适应阈值", cat:"process", icon:"threshold" },
        { type:"Conv",      title:"卷积",     desc:"自定义卷积核",       cat:"math",    icon:"conv"    }
    ]
    readonly property var categoryNames: ({
        input:"输入 / 输出", process:"图像处理", math:"数学 / 张量", output:"输出 / 显示"
    })
    // 已实现 QML 的类型 → 组件 URL；未实现返回 ""
    function componentUrl(type) {
        if(type === "ImageLoad") return "qrc:/ImageLoadNode.qml"
        if(type === "ImageShow") return "qrc:/ImageShowNode.qml"
        return ""
    }
    function byType(type) { return items.find(i => i.type === type) }
}
```
在 `assets.qrc` 加入 `NodeCatalog.qml`，并在 `src/main.cpp` 里模块注册（`import NodeCatalog` 使用即可；pragma Singleton 需在 qmldir 或通过 `QML_SINGLETON` 等价机制——本项目 QML 文件经 qrc 加载，单例需在 C++ 侧 `qmlRegisterSingletonType(QUrl("qrc:/NodeCatalog.qml"), "NodeCatalog", 1, 0, "NodeCatalog")`，在 `main.cpp` 添加该行）。

- [ ] **Step 2: 新建 `qml/palette/PaletteItem.qml`**

实现列表行 / 网格卡两种形态（`property bool gridMode`），含分类色图标框、标题、描述、悬停显示的 `＋` 按钮；信号 `addRequested(string type)`。图标用内联 SVG（`Shape` 或 `Image` + `data:`/qrc 资源），图标路径移植自 `ortdraw-gui-v4.html` 的 `ICONS` 常量。

- [ ] **Step 3: 新建 `qml/palette/NodePalette.qml`**

按 v4 原型实现：标题栏（列表/网格切换 + 折叠按钮）、搜索框（含清除）、分类筛选 chips、按 `category` 分组（组头三角 + 名称 + 数量，可折叠）、空结果提示；条目点击或 `＋` → 用 `NodeCatalog.componentUrl(type)` 创建对象并 `NodeManager.createNode(obj)`：

```qml
import QtQuick
import QtQuick.Controls
import Theme
import NodeCatalog
import NodeManager

Rectangle {
    id: palette
    color: Theme.bgPanel
    signal collapseRequested()

    function addNode(type) {
        const url = NodeCatalog.componentUrl(type)
        if(!url){ toast.show("该节点尚未实现"); return }
        const comp = Qt.createComponent(url)
        if(comp.status !== Component.Ready){ console.error(comp.errorString()); return }
        const obj = comp.createObject(palette.nodeLayer)
        if(obj) NodeManager.createNode(obj)
    }
    // ListView（列表）/ GridView（网格）按 gridMode 切换；分组折叠状态存 JS 对象
}
```
`nodeLayer` 从 `CanvasArea` 注入（通过 main.qml 的属性传递）。

- [ ] **Step 4: 构建、qmllint、回归**

Run:
```bash
cmake -S . -B build && cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/palette/*.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功、qmllint 无语法错误、测试通过。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add qml/palette assets.qrc src/main.cpp
git commit -m "feat: node catalog and palette drawer with list/grid, search and filters"
```

---

## Task 6: NodeManager 快照 / 计数 / 选择 / 动作接口

**Files:**
- Create: `include/utils/Snapshot.hpp`
- Create: `tests/test_snapshot.cpp`
- Modify: `include/NodeManager.h`
- Modify: `tests/CMakeLists.txt`（加入 `Snapshot.hpp` 头）

- [ ] **Step 1: 写失败测试 `tests/test_snapshot.cpp`**

```cpp
#include <QtTest>
#include "utils/Snapshot.hpp"
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "port/Port.hpp"

static Port* addPort(BaseNode* n, PortType t, DataType d){
    auto* p = new Port("p", t, d, QPointF(0,0), n);
    (t == PortType::Input ? n->getInPorts() : n->getOutPorts()).append(p);
    return p;
}

class TestSnapshot : public QObject {
    Q_OBJECT
private slots:
    void nodeSnapshotFields() {
        DAGraph g; auto* n = new BaseNode(); g.addNode(n);
        n->setX(10); n->setY(20); n->setWidth(220); n->setHeight(300);
        auto list = nodeSnapshotsOf(g);
        QCOMPARE(list.size(), 1);
        auto m = list.first().toMap();
        QVERIFY(m.contains("x")); QVERIFY(m.contains("y"));
        QVERIFY(m.contains("w")); QVERIFY(m.contains("h"));
        QVERIFY(m.contains("category")); QVERIFY(m.contains("selected"));
        QCOMPARE(m["x"].toReal(), 10.0);
        delete n;
    }
    void edgeSnapshotUsesPortPositions() {
        DAGraph g;
        auto* a = new BaseNode(); auto* b = new BaseNode();
        g.addNode(a); g.addNode(b);
        auto* out = addPort(a, PortType::Output, DataType::Image);
        auto* in  = addPort(b, PortType::Input,  DataType::Image);
        out->setPosition(QPointF(100, 50));
        in->setPosition(QPointF(300, 80));
        QVERIFY(g.addEdge(out, in));
        auto list = edgeSnapshotsOf(g);
        QCOMPARE(list.size(), 1);
        auto m = list.first().toMap();
        QCOMPARE(m["fromX"].toReal(), 100.0);
        QCOMPARE(m["fromY"].toReal(), 50.0);
        QCOMPARE(m["toX"].toReal(), 300.0);
        QCOMPARE(m["toY"].toReal(), 80.0);
        delete a; delete b;
    }
    void emptyGraphGivesEmptyLists() {
        DAGraph g;
        QVERIFY(nodeSnapshotsOf(g).isEmpty());
        QVERIFY(edgeSnapshotsOf(g).isEmpty());
    }
};

QTEST_MAIN(TestSnapshot)
#include "test_snapshot.moc"
```

- [ ] **Step 2: 运行确认失败**

Run: `cmake -S . -B build && cmake --build build -j4 --target test_snapshot 2>&1 | tail -20`
Expected: 编译失败，找不到 `utils/Snapshot.hpp`。

- [ ] **Step 3: 新建 `include/utils/Snapshot.hpp`**

```cpp
#pragma once
#include <QVariantList>
#include <QVariantMap>
#include "utils/DAGraph.hpp"
#include "node/BaseNode.hpp"
#include "utils/Edge.hpp"

inline QVariantList nodeSnapshotsOf(DAGraph& graph) {
    QVariantList out;
    for(BaseNode* n : graph.getAllNodes()){
        QVariantMap m;
        m["uuid"]     = n->uuid().toString();
        m["x"]        = n->x();
        m["y"]        = n->y();
        m["w"]        = n->width();
        m["h"]        = n->height();
        m["category"] = n->category();
        m["selected"] = n->selected();
        out.append(m);
    }
    return out;
}
inline QVariantList edgeSnapshotsOf(DAGraph& graph) {
    QVariantList out;
    for(const Edge& e : graph.getAllEdges()){
        if(!e.start_port || !e.stop_port) continue;
        QVariantMap m;
        m["fromX"]    = e.start_port->position().x();
        m["fromY"]    = e.start_port->position().y();
        m["toX"]      = e.stop_port->position().x();
        m["toY"]      = e.stop_port->position().y();
        m["selected"] = e.seleected;
        out.append(m);
    }
    return out;
}
```
在 `tests/CMakeLists.txt` 的 `TEST_HEADERS` 加入 `${PROJECT_SOURCE_DIR}/include/utils/Snapshot.hpp`。

- [ ] **Step 4: 运行测试确认通过**

Run: `cmake --build build -j4 --target test_snapshot && ctest --test-dir build -R test_snapshot --output-on-failure`
Expected: 3 个用例 PASS。

- [ ] **Step 5: `NodeManager` 增加属性、信号与动作**

在 `include/NodeManager.h` 中加入（保持现有方法不动）：

```cpp
#include "utils/Snapshot.hpp"

// private:
    BaseNode* m_selected_node = nullptr;
    void refresh(){ if(m_paint_board) m_paint_board->update(); emit graphChanged(); }
    void setSelectedNode(BaseNode* n){
        if(m_selected_node == n) return;
        m_selected_node = n; emit selectionChanged();
    }
// public:
    Q_PROPERTY(int nodeCount READ nodeCount NOTIFY graphChanged)
    Q_PROPERTY(int edgeCount READ edgeCount NOTIFY graphChanged)
    Q_PROPERTY(BaseNode* selectedNode READ selectedNode NOTIFY selectionChanged)
    Q_PROPERTY(QVariantMap selectedEdge READ selectedEdge NOTIFY selectionChanged)

    int nodeCount() const { return m_paint_board ? m_paint_board->m_graph.getAllNodes().size() : 0; }
    int edgeCount() const { return m_paint_board ? m_paint_board->m_graph.getAllEdges().size() : 0; }
    BaseNode* selectedNode() const { return m_selected_node; }
    QVariantMap selectedEdge() const {
        QVariantMap m;
        if(!m_paint_board) return m;
        for(const Edge& e : m_paint_board->m_graph.getSelectedEdges()){
            m["from"] = e.start_port->father()->uuid().toString();
            m["fromPort"] = 0; m["to"] = e.stop_port->father()->uuid().toString(); m["toPort"] = 0;
            break;
        }
        return m;
    }
    Q_INVOKABLE QVariantList nodeSnapshots() const {
        if(!m_paint_board) return {};
        auto* self = const_cast<NodeManager*>(this);
        return nodeSnapshotsOf(self->m_paint_board->m_graph);
    }
    Q_INVOKABLE QVariantList edgeSnapshots() const {
        if(!m_paint_board) return {};
        auto* self = const_cast<NodeManager*>(this);
        return edgeSnapshotsOf(self->m_paint_board->m_graph);
    }
    Q_INVOKABLE void bringToFront(QUuid uid){
        if(!m_paint_board) return;
        for(auto* n : m_paint_board->m_graph.getAllNodes())
            if(n->uuid() == uid) n->setZ(1);
        refresh();
    }
    Q_INVOKABLE void disconnectNode(QUuid uid){
        if(!m_paint_board) return;
        BaseNode* target = nullptr;
        for(auto* n : m_paint_board->m_graph.getAllNodes())
            if(n->uuid() == uid) { target = n; break; }
        if(!target) return;
        for(const Edge& e : m_paint_board->m_graph.edgesOf(target)){
            auto cmd = std::make_unique<RemoveEdgeCMD>(e.start_port, e.stop_port, m_paint_board);
            m_cmd_manager.executeCommand(std::move(cmd));
        }
        refresh();
    }
    Q_INVOKABLE void clearGraph(){
        if(!m_paint_board) return;
        auto nodes = m_paint_board->m_graph.getAllNodes();
        for(auto* n : nodes){
            auto cmd = std::make_unique<RemoveNodeCMD>(n, m_paint_board);
            m_cmd_manager.executeCommand(std::move(cmd));
        }
        setSelectedNode(nullptr);
        refresh();
    }
signals:
    void graphChanged();
    void selectionChanged();
```

并在 `clickNodeEvent`/`mousePressEvent` 末尾更新 `setSelectedNode(...)`（命中节点则设为该节点，否则 `nullptr`）。

- [ ] **Step 6: 构建与全量测试**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: 7 个测试套件全部通过。

- [ ] **Step 7: Commit（待用户授权）**

```bash
git add include/utils/Snapshot.hpp tests/test_snapshot.cpp tests/CMakeLists.txt include/NodeManager.h
git commit -m "feat: NodeManager snapshots, counts, selection and graph actions"
```

---

## Task 7: Minimap

**Files:**
- Create: `qml/canvas/Minimap.qml`
- Modify: `assets.qrc`

- [ ] **Step 1: 新建 `qml/canvas/Minimap.qml`**

用 `Canvas` 绘制，数据来自 `NodeManager.nodeSnapshots()/edgeSnapshots()`，在 `NodeManager.graphChanged` 时重绘；视口矩形由 `zoom/panX/panY` 与组件尺寸换算。世界坐标→minimap 坐标公式（与原型一致）：

```qml
import QtQuick
import Theme
import NodeManager

Rectangle {
    id: mini
    width: 210; height: 140
    radius: 9
    color: Theme.bgPanel
    border.color: Theme.border
    property var world
    property real zoom: 1
    property real panX: 0
    property real panY: 0
    signal jumpTo(real wx, real wy)

    Canvas {
        id: cv
        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d"); ctx.reset()
            const W = width, H = height
            const nodes = NodeManager.nodeSnapshots()
            const edges = NodeManager.edgeSnapshots()
            // 计算包围盒：节点 ∪ 当前视口
            const vw = (mini.parent.width / mini.zoom)
            const vh = (mini.parent.height / mini.zoom)
            const vx = -mini.panX / mini.zoom, vy = -mini.panY / mini.zoom
            let minX = vx, minY = vy, maxX = vx + vw, maxY = vy + vh
            for(const n of nodes){ minX=Math.min(minX,n.x); minY=Math.min(minY,n.y)
                                   maxX=Math.max(maxX,n.x+n.w); maxY=Math.max(maxY,n.y+n.h) }
            const pad = 80
            minX-=pad; minY-=pad; maxX+=pad; maxY+=pad
            const s = Math.min(W/(maxX-minX), H/(maxY-minY))
            const ox = (W-(maxX-minX)*s)/2 - minX*s
            const oy = (H-(maxY-minY)*s)/2 - minY*s
            mini._map = { s, ox, oy }
            // 连线
            ctx.strokeStyle = Theme.wire; ctx.lineWidth = 1
            for(const e of edges){ ctx.beginPath()
                ctx.moveTo(e.fromX*s+ox, e.fromY*s+oy); ctx.lineTo(e.toX*s+ox, e.toY*s+oy); ctx.stroke() }
            // 节点
            for(const n of nodes){
                ctx.fillStyle = n.category==="input"?Theme.catInput : n.category==="math"?Theme.catMath
                              : n.category==="output"?Theme.catOutput : Theme.catProcess
                ctx.fillRect(n.x*s+ox, n.y*s+oy, Math.max(2,n.w*s), Math.max(2,n.h*s))
            }
            // 视口
            ctx.strokeStyle = Theme.blue; ctx.lineWidth = 1.4
            ctx.strokeRect(vx*s+ox, vy*s+oy, vw*s, vh*s)
        }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: (e) => { const m = mini._map
            mini.jumpTo((e.x-m.ox)/m.s, (e.y-m.oy)/m.s) }
    }
    property var _map: ({s:1, ox:0, oy:0})
    Connections { target: NodeManager; function onGraphChanged(){ cv.requestPaint() } }
    Connections { target: Theme; function onChanged(){ cv.requestPaint() } }
    onZoomChanged: cv.requestPaint(); onPanXChanged: cv.requestPaint(); onPanYChanged: cv.requestPaint()
}
```
`CanvasArea` 接收 `jumpTo` 并据此调整 `panX/panY`（让该点居中），并替换 Task 4 中的空壳 Minimap。

- [ ] **Step 2: 构建、qmllint、回归**

Run:
```bash
cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/canvas/Minimap.qml qml/canvas/CanvasArea.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功、qmllint 无语法错误、测试通过。

- [ ] **Step 3: Commit（待用户授权）**

```bash
git add qml/canvas/Minimap.qml assets.qrc
git commit -m "feat: minimap with viewport and click-to-navigate"
```

---

## Task 8: 右键上下文菜单

**Files:**
- Create: `qml/UiBus.qml`（pragma Singleton，信号总线）
- Create: `qml/chrome/ContextMenu.qml`
- Modify: `qml/main.qml`、`qml/node/NodeCard.qml`、`qml/canvas/CanvasArea.qml`
- Modify: `src/main.cpp`、`assets.qrc`

- [ ] **Step 1: 新建 `qml/UiBus.qml`**

```qml
pragma Singleton
import QtQuick

QtObject {
    signal contextMenuRequested(real x, real y, string kind, var data)
}
```
C++ 注册：`qmlRegisterSingletonType(QUrl("qrc:/UiBus.qml"), "UiBus", 1, 0, "UiBus");`

- [ ] **Step 2: 新建 `qml/chrome/ContextMenu.qml`**

基于 `Menu` 实现三种上下文（`kind`：`node`/`edge`/`canvas`），条目与动作见 spec 第 9 节。动作调用：`NodeManager.removeNode()`、`NodeManager.removeEdge()`、`NodeManager.disconnectNode(uid)`、`NodeManager.bringToFront(uid)`、`NodeManager.clearGraph()`、`canvas.fitView()`；"复制节点"由 QML 按 `typeName` 经 `NodeCatalog` 创建并 `createNode`；"添加节点▸"子菜单列出 `NodeCatalog.items`。

```qml
import QtQuick
import QtQuick.Controls
import Theme
import NodeManager
import UiBus
import NodeCatalog

Menu {
    id: root
    property string kind: "canvas"
    property var data: ({})
    MenuItem { text: "复制节点"; visible: root.kind==="node"; onTriggered: root.cloneNode() }
    MenuItem { text: "断开全部连接"; visible: root.kind==="node"; onTriggered: NodeManager.disconnectNode(root.data.uid) }
    MenuItem { text: "置顶"; visible: root.kind==="node"; onTriggered: NodeManager.bringToFront(root.data.uid) }
    MenuSeparator { visible: root.kind==="node" }
    MenuItem { text: "删除节点"; visible: root.kind==="node"; onTriggered: NodeManager.removeNode() }
    MenuItem { text: "删除连线"; visible: root.kind==="edge"; onTriggered: NodeManager.removeEdge() }
    Menu { title: "添加节点"; visible: root.kind==="canvas"; Instantiator { model: NodeCatalog.items; delegate: MenuItem { text: modelData.title; onTriggered: root.addNode(modelData.type) } } }
    MenuItem { text: "适应视图"; visible: root.kind==="canvas"; onTriggered: root.fitRequested() }
    MenuItem { text: "清空画布"; visible: root.kind==="canvas"; onTriggered: NodeManager.clearGraph() }
    signal fitRequested()
    function cloneNode(){ /* 按 NodeManager.selectedNode.typeName 查找组件、偏移创建 */ }
    function addNode(type){ /* 同 NodePalette.addNode */ }
    Connections { target: UiBus
        function onContextMenuRequested(x,y,kind,data){ root.kind=kind; root.data=data; root.popup(x,y) } }
}
```

- [ ] **Step 3: 在节点/连线/画布上触发**

- `NodeCard.qml`：`MouseArea { acceptedButtons: Qt.LeftButton | Qt.RightButton; onClicked: if(mouse.button === Qt.RightButton) UiBus.contextMenuRequested(mapToItem(null, mouse.x, mouse.y).x, mapToItem(null, mouse.y*0+mapToItem(null, mouse.x, mouse.y).y, "node", {uid: node.uuid})` —— 简化实现：用 `mapToItem(null, mouse.x, mouse.y)` 得全局坐标后发出。
- `CanvasArea.qml` 的 `PaintBoard` 连线命中：在画布的 `MouseArea.onPressed` 中若 `button === Qt.RightButton`，先调用 `NodeManager.mousePressEvent(pos, false)` 让连线/节点完成命中与选择，再发 `UiBus.contextMenuRequested(..., "edge" 或 "canvas", ...)`。
- `main.qml` 实例化 `ContextMenu { onFitRequested: canvas.fitView() }`。

- [ ] **Step 4: 构建、qmllint、回归**

Run:
```bash
cmake -S . -B build && cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/UiBus.qml qml/chrome/ContextMenu.qml qml/node/NodeCard.qml qml/canvas/CanvasArea.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功、qmllint 无语法错误、测试通过。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add qml/UiBus.qml qml/chrome/ContextMenu.qml qml/node/NodeCard.qml qml/canvas/CanvasArea.qml qml/main.qml src/main.cpp assets.qrc
git commit -m "feat: context menu for node, edge and canvas"
```

---

## Task 9: 属性面板

**Files:**
- Create: `qml/inspector/Inspector.qml`
- Modify: `qml/main.qml`、`assets.qrc`

- [ ] **Step 1: 新建 `qml/inspector/Inspector.qml`**

绑定 `NodeManager.selectedNode` 与 `NodeManager.selectedEdge`：

```qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Theme
import NodeManager

Rectangle {
    id: insp
    color: Theme.bgPanel
    signal collapseRequested()

    property var node: NodeManager.selectedNode
    property var edge: NodeManager.selectedEdge

    ColumnLayout {
        anchors.fill: parent; spacing: 0
        // 头部：折叠按钮 + "属性" + 类型徽章
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            ColumnLayout {
                width: insp.width - 28
                // 未选中：占位
                Text { visible: !insp.node && !insp.edge; text: "选择一个节点查看属性"; color: Theme.fgDim }
                // 选中节点
                ColumnLayout {
                    visible: !!insp.node
                    TextField { text: insp.node ? insp.node.name : ""
                                onEditingFinished: if(insp.node) insp.node.name = text }
                    // 端口 chips（Repeater 遍历 node.inputPorts / outputPorts）
                    // 位置 X/Y、分类色板
                }
                // 选中连线
                ColumnLayout { visible: !insp.node && insp.edge && insp.edge.from !== undefined
                    Text { text: "已选中连线" ; color: Theme.fg }
                }
            }
        }
    }
}
```
细节参照 `ortdraw-gui-v4.html` 的 `.inspector` 区块（字段、chips、section 标题、提示卡）。

- [ ] **Step 2: `main.qml` 替换 Inspector 空壳**为真实组件，并把折叠信号接到 `rightCollapsed`。

- [ ] **Step 3: 构建、qmllint、回归**

Run:
```bash
cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/inspector/Inspector.qml qml/main.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功、qmllint 无语法错误、测试通过。

- [ ] **Step 4: Commit（待用户授权）**

```bash
git add qml/inspector/Inspector.qml qml/main.qml assets.qrc
git commit -m "feat: inspector bound to selection"
```

---

## Task 10: 文档、整体校验与冒烟

**Files:**
- Modify: `README.md`

- [ ] **Step 1: 更新 `README.md`**

在功能与目录结构中补充：亮/暗主题（默认亮色）、节点库（列表/网格、搜索、筛选、分组折叠）、属性面板、minimap、右键菜单；`include/Theme.h`、`qml/palette`、`qml/canvas`、`qml/chrome`、`qml/inspector` 目录说明；主题切换快捷键/按钮；并注明 GUI 冒烟测试需图形环境。

- [ ] **Step 2: 全新构建 + 全量测试**

Run:
```bash
rm -rf build && cmake -S . -B build && cmake --build build -j4
ctest --test-dir build --output-on-failure
```
Expected: 构建成功（无 warning），8 个测试套件（port/dagraph/edge/paintboard/cmdmanager/theme/snapshot + 原有）全部 PASS。

- [ ] **Step 3: qmllint 全量**

Run:
```bash
/usr/lib/qt6/bin/qmllint $(find qml -name '*.qml')
```
Expected: 无语法错误（未注册类型的 unresolved 警告可忽略）。

- [ ] **Step 4: 手工冒烟（需图形环境，无法在无显示环境执行）**

`./bin/main` 后逐项验证：主题切换（默认亮色）、左右面板折叠与窄屏自动收起、节点库列表/网格切换、搜索与分类筛选、分组折叠、从节点库添加节点、拖拽移动/缩放、端口连线、选中与多选、Delete 删除、Ctrl+Z/Y、minimap 点击定位、三种右键菜单、属性面板改名。

- [ ] **Step 5: Commit（待用户授权）**

```bash
git add README.md
git commit -m "docs: document redesigned GUI"
```

---

## 自查记录

- **Spec 覆盖**：主题系统→Task 1/2；节点卡与节点视觉→Task 3；布局/顶部栏/状态栏/折叠→Task 4；节点库→Task 5；minimap 数据→Task 6，渲染→Task 7；右键菜单→Task 8；属性面板→Task 9；文档与校验→Task 10。spec 第 3–13 节均有对应任务。
- **占位符**：无 TBD/TODO；C++ 与测试给出完整代码；QML 因体量较大，给出关键代码 + 明确以 `ortdraw-gui-v4.html` 为逐条移植基准（该文件是仓库内真实且可运行的权威视觉源）。
- **类型一致性**：`Theme` token 名（`bg/bgPanel/.../catOutput`）、`BaseNode::typeName/category/description`、`NodeManager::{nodeCount,edgeCount,nodeSnapshots,edgeSnapshots,selectedNode,selectedEdge,disconnectNode,bringToFront,clearGraph,graphChanged,selectionChanged}`、`nodeSnapshotsOf/edgeSnapshotsOf`、`UiBus.contextMenuRequested` 在各任务中名称一致。
- **回归约束**：每个 Task 结束都必须保持现有 5 个测试套件通过；Task 1/6 新增 `test_theme`/`test_snapshot`。

