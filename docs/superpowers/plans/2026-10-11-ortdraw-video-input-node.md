# 视频输入节点（单帧 + 节点内播放条）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增「视频输入」节点：从本地视频按帧号取一帧输出 `Image`，卡片内播放条可播放/暂停/seek/调 fps/循环，播放时按帧率触发一次全图运行，从而把视频逐帧送入现有链路（传统算子 / 预处理 → ONNX → 后处理）。

**Architecture:** 完全复用现有节点范式与一次一跑的引擎：`VideoInputExecutor` 用 `cv::VideoCapture`（按 path 缓存）+ `CAP_PROP_POS_FRAMES` seek + `read()` 取一帧；`VideoInputNode` 承载 path/fps/loop/frame 与视频信息；QML 播放条用 `Timer` 推进帧号并调用 `NodeManager.run()`。**引擎零改动**。

**Tech Stack:** C++23 / Qt6 Quick（QML）/ OpenCV 5（`opencv_videoio`）/ QtTest。

**Spec:** `docs/superpowers/specs/2026-10-11-ortdraw-video-input-node-design.md`

## Global Constraints

- 仓库约定：**只有用户明确说「提交」时才执行 `git commit`**；每个 Task 末尾先汇报并等待确认。不得用 `git checkout`/`git stash` 还原用户未提交的改动。
- 字符串拼接一律 `std::format`（C++ 侧）；QML 侧用既有 `Theme.*`/`Settings.*`，不写死颜色。
- `NodeData = variant<monostate, cv::Mat, Tensor, double, bool>`；端口类型 `Image/Tensor/Number/Bool/Any`；执行器签名 `ExecResult execute(const ExecuteContext&, const QVariantMap& params, const QVector<NodeData>& inputs) const`。
- 构建：`cmake --build build -j4`；测试：`ctest --test-dir build --output-on-failure`（当前 23/23，本计划新增 1 个）；QML 检查：`/usr/lib/qt6/bin/qmllint <file>`。
- QML 打进 qrc：改 QML 后必须重新构建。
- 测试素材 `assets/test.mkv`（1744×982、30.303fps、2863 帧、约 36MB）**不入库**（已在 `.gitignore`）；测试在该文件缺失时 `QSKIP`。
- 节点宽度 240（`min_width`），高度沿用 `Settings.autoHeight` 自适应。
- 帧号（`frame`）**不进撤销栈**（播放推进与手动 seek 都不进）；可撤销参数只有 `path/fps/loop`。

## Review Focus

以下输入/条件 spec 未逐条给测试，但最可能咬到用户，已挂在对应 Task 的步骤里：

1. **视频文件被移动/删除/无解码器**（含保存的图换机器后打开）→ 明确错误文案、播放条禁用、不崩溃：Task 1 单测 + Task 4 人工。
2. **播放到尾帧之后**（loop 开/关）→ 回绕或停止，不死循环、不报错刷屏：Task 1 单测（越界）+ Task 3 人工。
3. **上一帧还没跑完就又到下一帧**（fps 高于单帧耗时）→ 跳过该帧，不堆积运行、不卡 UI：Task 3（`engineRunning()` 守卫）。
4. **播放中切换视频/关闭图/取消运行** → 不崩溃、`VideoCapture` 缓存可重建、定时器随之停止：Task 4 人工。
5. **`frameCount` 未知（0）的视频** → 滑块禁用、只显示帧号、播放按读失败即停：Task 3 人工（可用 `frameCount==0` 的构造难以稳定复现，按代码审查 + Task 4 抽查）。

---

## 文件结构

- Create: `include/engine/executors/VideoInputExecutor.hpp`（取帧执行器 + capture 缓存）
- Create: `include/node/VideoInput.hpp`（节点：path/fps/loop/frame + 视频信息）
- Create: `qml/node/VideoInputNode.qml`（卡片：选文件 + 信息行 + 播放条 + loop）
- Create: `tests/test_video_input.cpp`（执行器单测）
- Modify: `include/FileDialogs.h`（`openVideo`）
- Modify: `include/engine/BuiltinExecutors.hpp`（注册执行器）
- Modify: `qml/palette/NodeCatalog.qml`（目录条目 + componentUrl）
- Modify: `qml/palette/PaletteItem.qml`（`video` 图标）
- Modify: `assets.qrc`、`src/main.cpp`（qrc + QML 类型注册）
- Modify: `tests/CMakeLists.txt`（`ORTDRAW_ASSETS_DIR` 定义）

---

## Task 1: 视频取帧执行器 + 单测（C++ 后端）

**Files:**
- Create: `include/engine/executors/VideoInputExecutor.hpp`
- Create: `tests/test_video_input.cpp`
- Modify: `tests/CMakeLists.txt`（在既有 `target_compile_definitions` 里加一行）

**Interfaces:**
- Consumes: `engine/NodeExecutor.hpp`（`ExecResult`/`ExecuteContext`）、`engine/NodeData.hpp`（`NodeData` 可存 `cv::Mat`）、OpenCV `opencv2/videoio.hpp`。
- Produces: `class VideoInputExecutor : public NodeExecutor`（`execute()` 读 `params["path"]`、`params["frame"]`，输出一帧 `cv::Mat`）；错误文案固定为：`未选择视频文件` / `无法打开视频（缺解码器？）: <path>` / `帧号超出范围（共 N 帧）` / `seek 到第 N 帧失败` / `读取第 N 帧失败`。Task 2/3 依赖这些文案做测试与提示。

- [ ] **Step 1: 写失败测试**

创建 `tests/test_video_input.cpp`：

```cpp
#include <QtTest>
#include <QFileInfo>
#include "engine/executors/VideoInputExecutor.hpp"

class TestVideoInput : public QObject {
    Q_OBJECT
private slots:
    // 未选择文件：明确报错，不崩溃
    void emptyPathFails() {
        VideoInputExecutor ex;
        const ExecResult r = ex.execute({}, QVariantMap{}, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("未选择视频")));
        QVERIFY(r.outputs.isEmpty());
    }

    // 文件不存在 / 打不开：明确报错
    void missingFileFails() {
        VideoInputExecutor ex;
        QVariantMap p;
        p["path"] = QStringLiteral("/tmp/opencode/no-such-video.mkv");
        p["frame"] = 0;
        const ExecResult r = ex.execute({}, p, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("无法打开")));
    }

    // 真实素材：帧号越界必须报错（而不是返回空图或崩溃）
    void frameOutOfRangeFails() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        QVariantMap p;
        p["path"] = path;
        p["frame"] = 999999;
        const ExecResult r = ex.execute({}, p, {});
        QVERIFY(!r.ok);
        QVERIFY(r.error.contains(QStringLiteral("帧号超出范围")));
    }

    // 真实素材：seek 到不同帧都能读出 1744x982 的 BGR 8UC3
    void readsFramesFromAsset() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        for (int f : {0, 30, 900}) {
            QVariantMap p;
            p["path"] = path;
            p["frame"] = f;
            const ExecResult r = ex.execute({}, p, {});
            QVERIFY2(r.ok, r.error.toStdString().c_str());
            QCOMPARE(r.outputs.size(), 1);
            const cv::Mat m = std::get<cv::Mat>(r.outputs[0]);   // 与仓库既有测试一致
            QCOMPARE(m.cols, 1744);
            QCOMPARE(m.rows, 982);
            QCOMPARE(m.type(), CV_8UC3);
            QVERIFY(!m.empty());
        }
    }

    // 两次读同一帧结果一致（seek 语义稳定）
    void sameFrameIsStable() {
        const QString path = QStringLiteral(ORTDRAW_ASSETS_DIR) + "/test.mkv";
        if (!QFileInfo::exists(path)) QSKIP("assets/test.mkv 不存在（测试素材不入库）");
        VideoInputExecutor ex;
        auto readAt = [&](int f) {
            QVariantMap p;
            p["path"] = path;
            p["frame"] = f;
            const ExecResult r = ex.execute({}, p, {});
            return std::get<cv::Mat>(r.outputs[0]).clone();
        };
        const cv::Mat a = readAt(120);
        const cv::Mat b = readAt(120);
        QCOMPARE(cv::norm(a, b), 0.0);
    }
};

QTEST_MAIN(TestVideoInput)
#include "test_video_input.moc"
```

- [ ] **Step 2: 运行测试确认失败**

Run: `cmake --build build -j4 && ctest --test-dir build -R test_video_input --output-on-failure`
Expected: 编译失败（`VideoInputExecutor.hpp` 不存在），测试无法构建。

- [ ] **Step 3: 加 `ORTDRAW_ASSETS_DIR` 定义**

`tests/CMakeLists.txt` 中既有：

```cmake
    target_compile_definitions($${name} PRIVATE
        ORTDRAW_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
        ORTDRAW_MODELS_DIR="${PROJECT_SOURCE_DIR}/models"
    )
```

改为（加一行，其余不动）：

```cmake
    target_compile_definitions(${name} PRIVATE
        ORTDRAW_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data"
        ORTDRAW_MODELS_DIR="${PROJECT_SOURCE_DIR}/models"
        ORTDRAW_ASSETS_DIR="${PROJECT_SOURCE_DIR}/assets"
    )
```

- [ ] **Step 4: 实现执行器**

创建 `include/engine/executors/VideoInputExecutor.hpp`：

```cpp
#pragma once
#include <opencv2/videoio.hpp>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "engine/NodeExecutor.hpp"

// 视频输入执行器：按帧号取一帧输出（BGR 8UC3，与 cv::imread 一致）。
// 同一路径的 cv::VideoCapture 会被缓存，避免每帧重新打开文件（seek 用
// CAP_PROP_POS_FRAMES）。图求值是单线程拓扑序，mutex 仅作防御性保护。
class VideoInputExecutor : public NodeExecutor {
public:
    ExecResult execute(const ExecuteContext&,
                       const QVariantMap& params,
                       const QVector<NodeData>&) const override {
        const QString path = params.value("path").toString();
        if (path.isEmpty())
            return {false, QStringLiteral("未选择视频文件"), {}};

        const int frame = params.value("frame", 0).toInt();

        std::shared_ptr<cv::VideoCapture> cap = captureFor(path);
        if (!cap->isOpened())
            return {false, QStringLiteral("无法打开视频（缺解码器？）: %1").arg(path), {}};

        const double total = cap->get(cv::CAP_PROP_FRAME_COUNT);
        if (frame < 0 || (total > 0 && frame >= static_cast<int>(total)))
            return {false, QStringLiteral("帧号超出范围（共 %1 帧）")
                               .arg(total > 0 ? QString::number(static_cast<int>(total))
                                              : QStringLiteral("?")), {}};

        if (!cap->set(cv::CAP_PROP_POS_FRAMES, frame))
            return {false, QStringLiteral("seek 到第 %1 帧失败").arg(frame), {}};

        cv::Mat img;
        if (!cap->read(img) || img.empty())
            return {false, QStringLiteral("读取第 %1 帧失败").arg(frame), {}};

        ExecResult r;
        r.outputs.push_back(img);
        return r;
    }

private:
    // 按路径缓存 VideoCapture；打不开的条目不缓存（文件被删/缺解码器时下次可重试）
    static std::shared_ptr<cv::VideoCapture> captureFor(const QString& path) {
        static std::mutex mutex;
        static std::unordered_map<std::string, std::shared_ptr<cv::VideoCapture>> cache;
        std::lock_guard<std::mutex> lk(mutex);

        const std::string key = path.toStdString();
        auto it = cache.find(key);
        if (it != cache.end()) {
            if (it->second->isOpened()) return it->second;
            cache.erase(it);          // 缓存的 capture 已失效：重建
        }
        auto cap = std::make_shared<cv::VideoCapture>(key);
        if (cap->isOpened()) cache[key] = cap;
        return cap;                   // 打不开也返回，由调用方报错
    }
};
```

- [ ] **Step 5: 运行测试确认通过**

Run: `cmake --build build -j4 && ctest --test-dir build --output-on-failure`
Expected: `100% tests passed out of 24`（新增 `test_video_input` 5 个用例全过；`assets/test.mkv` 缺失时相关用例 QSKIP 而非失败）。

- [ ] **Step 6: 汇报并等待用户确认后提交**

汇报：新增执行器与 5 个单测、`ORTDRAW_ASSETS_DIR`、测试结果。用户说「提交」后执行：
```bash
git add include/engine/executors/VideoInputExecutor.hpp tests/test_video_input.cpp tests/CMakeLists.txt
git commit -m "feat(video): frame-reading executor with capture cache and unit tests"
```

---

## Task 2: 节点类、文件对话框与注册（可选中视频、显示信息、参数持久化）

**Files:**
- Create: `include/node/VideoInput.hpp`
- Modify: `include/FileDialogs.h`
- Modify: `include/engine/BuiltinExecutors.hpp`
- Modify: `qml/palette/NodeCatalog.qml`
- Modify: `qml/palette/PaletteItem.qml`（`video` 图标）
- Modify: `assets.qrc`、`src/main.cpp`

**Interfaces:**
- Consumes: Task 1 的 `VideoInputExecutor`（注册名 `"VideoInput"`）。
- Produces: `class VideoInputNode : public BaseNode`，QML 属性 `path/fps/loop/frame/infoText/frameCount/videoValid`；`FileDialogs.openVideo(startDir)`；palette 条目 `type:"VideoInput"`。Task 3 的 QML 依赖这些属性名与文案。

- [ ] **Step 1: 实现节点类**

创建 `include/node/VideoInput.hpp`：

```cpp
#pragma once
#include <QFileInfo>
#include <QLocale>
#include <opencv2/videoio.hpp>
#include "node/BaseNode.hpp"

// 视频输入：从本地视频文件按帧号取一帧，输出 Image（BGR 8UC3）。
// 播放进度（frame）由 QML 播放条推进，不进撤销栈；可撤销参数只有 path/fps/loop。
class VideoInputNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY loopChanged)
    Q_PROPERTY(int frame READ frame WRITE setFrame NOTIFY frameChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY infoChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY infoChanged)
    Q_PROPERTY(bool videoValid READ videoValid NOTIFY infoChanged)
public:
    QString typeName() const override { return "VideoInput"; }
    QString category() const override { return "input"; }

    QString path() const { return m_path; }
    void setPath(const QString& p) {
        if (m_path == p) return;
        m_path = p;
        m_frame = 0;                     // 换文件从第 0 帧开始
        refreshInfo();
        emit pathChanged();
        emit frameChanged();
    }

    int fps() const { return m_fps; }
    void setFps(int v) {
        if (!(v >= 1)) v = 1;            // 同时挡住 NaN
        if (v > 120) v = 120;
        if (m_fps == v) return;
        m_fps = v;
        emit fpsChanged();
    }

    bool loop() const { return m_loop; }
    void setLoop(bool v) {
        if (m_loop == v) return;
        m_loop = v;
        emit loopChanged();
    }

    int frame() const { return m_frame; }
    void setFrame(int v) {
        if (v < 0) v = 0;
        if (m_frame == v) return;
        m_frame = v;
        emit frameChanged();
    }

    int frameCount() const { return m_frameCount; }
    bool videoValid() const { return m_valid; }
    QString infoText() const { return m_infoText; }

    QVariantMap params() const override {
        return {{"path", m_path}, {"fps", m_fps}, {"loop", m_loop}, {"frame", m_frame}};
    }
    void setParams(const QVariantMap& p) override {
        if (p.contains("path")) setPath(p.value("path").toString());   // 会把 frame 归零
        if (p.contains("fps")) setFps(p.value("fps").toInt());
        if (p.contains("loop")) setLoop(p.value("loop").toBool());
        if (p.contains("frame")) setFrame(p.value("frame").toInt());   // 必须放在 path 之后
    }

    VideoInputNode(QQuickItem* parent = nullptr) : BaseNode(parent) {
        m_name = QStringLiteral("视频输入");
        m_description = QStringLiteral("读取视频文件的一帧，输出图像；可按帧率播放。");
        min_width = 240;
        m_output_ports.push_back(new Port(QStringLiteral("输出"), PortType::Output,
                                          DataType::Image, QPointF(0, 0), this));
        refreshInfo();
    }

private:
    // 打开一次读元信息；不持有 capture（执行器自己缓存）
    void refreshInfo() {
        m_frameCount = 0;
        m_valid = false;
        m_infoText.clear();
        if (m_path.isEmpty()) { emit infoChanged(); return; }

        const QFileInfo fi(m_path);
        if (!fi.exists() || !fi.isFile()) {
            m_infoText = QStringLiteral("文件不存在");
            emit infoChanged();
            return;
        }
        cv::VideoCapture cap(m_path.toStdString());
        if (!cap.isOpened()) {
            m_infoText = QStringLiteral("无法打开（缺解码器？）");
            emit infoChanged();
            return;
        }
        const int w = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_WIDTH));
        const int h = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        const double fps = cap.get(cv::CAP_PROP_FPS);
        const int n = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        m_frameCount = n;
        m_valid = true;
        m_infoText = QStringLiteral("%1 × %2  ·  %3 FPS  ·  %4 帧  ·  %5")
                         .arg(w).arg(h)
                         .arg(fps, 0, 'f', 1)
                         .arg(n > 0 ? QString::number(n) : QStringLiteral("?"))
                         .arg(QLocale().formattedDataSize(fi.size()));
        emit infoChanged();
    }

signals:
    void pathChanged();
    void fpsChanged();
    void loopChanged();
    void frameChanged();
    void infoChanged();

private:
    QString m_path;
    int m_fps = 25;
    bool m_loop = true;
    int m_frame = 0;
    int m_frameCount = 0;
    bool m_valid = false;
    QString m_infoText;
};
```

- [ ] **Step 2: 加文件对话框**

`include/FileDialogs.h` 中 `openImage` 之后新增：

```cpp
    Q_INVOKABLE QString openVideo(const QString& startDir = QString()) {
        QString vids = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);
        if (vids.isEmpty() || !QDir(vids).exists()) vids = QDir::homePath();
        const QString start = startDir.isEmpty() ? vids : startDir;
        return QFileDialog::getOpenFileName(nullptr, tr("选择视频"), start,
            QStringLiteral("视频 (*.mp4 *.mov *.avi *.mkv *.webm *.flv *.ts *.m4v);;所有文件 (*)"));
    }
```

- [ ] **Step 3: 注册执行器、QML 类型、目录条目、图标**

`include/engine/BuiltinExecutors.hpp`：加 `#include "engine/executors/VideoInputExecutor.hpp"`，并在 `registerBuiltinExecutors()` 里 `ImageLoad` 一行之后加：

```cpp
    r.registerExecutor("VideoInput", std::make_shared<VideoInputExecutor>());
```

`src/main.cpp`：加 `#include "node/VideoInput.hpp"`，并在 `qmlRegisterType<ImageLoadNode>(...)` 之后加：

```cpp
    qmlRegisterType<VideoInputNode>("VideoInputNode", 1, 0, "VideoInputNode");
```

`assets.qrc`：在 `<file alias="ImageLoadNode.qml">qml/node/ImageLoadNode.qml</file>` 之后加：

```xml
        <file alias="VideoInputNode.qml">qml/node/VideoInputNode.qml</file>
```

`qml/palette/NodeCatalog.qml`：`items` 数组中 `ImageLoad` 条目之后加：

```qml
        { type:"VideoInput", title:"视频输入", desc:"读取视频文件的一帧（视频 / 摄像头 / 帧）", cat:"input", icon:"video" },
```

`componentUrl()` 中 `ImageLoad` 分支之后加：

```qml
        if (type === "VideoInput") return "qrc:/VideoInputNode.qml"
```

`qml/palette/PaletteItem.qml` 的 `paintIcon()` 中，`if (name === "image") { ... }` 分支之后插入：

```qml
        } else if (name === "video") {
            rr(3, 5, 18, 14, 2.5); ctx.stroke()
            line(3, 9.5, 21, 9.5)
            line(3, 14.5, 21, 14.5)
            ctx.beginPath(); ctx.moveTo(10, 8.5); ctx.lineTo(17, 12); ctx.lineTo(10, 15.5)
            ctx.closePath(); ctx.stroke()
```

（注意 `qml/node/VideoInputNode.qml` 此时还不存在，`assets.qrc` 引用它会在构建期报错——所以 Task 3 必须先建该文件；若 Task 2 需要单独构建通过，可先在 Task 2 建一个最小版 QML（只有「选择视频」按钮 + 信息行），Task 3 再补播放条。推荐后者，便于分阶段验证。）

- [ ] **Step 4: 最小 QML（先让节点可用）**

创建 `qml/node/VideoInputNode.qml`（Task 3 会扩成完整播放条）：

```qml
import QtQuick
import VideoInputNode
import Theme
import Settings
import FileDialogs
import NodeManager

VideoInputNode {
    id: root
    width: 240
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 4

            Row {
                width: parent.width
                spacing: 6
                Rectangle {
                    id: pickBtn
                    anchors.verticalCenter: parent.verticalCenter
                    width: pickLabel.implicitWidth + 20
                    height: 24
                    radius: 6
                    color: pickHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: pickHover.hovered ? Theme.blue : Theme.border
                    Text {
                        id: pickLabel
                        anchors.centerIn: parent
                        text: "选择视频"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: root.textRenderType
                    }
                    HoverHandler { id: pickHover }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const p = FileDialogs.openVideo(root.path)
                            if (p !== "") {
                                root.path = p
                                NodeManager.commitNodeParams(root.uuid)
                                NodeManager.run()
                            }
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - pickBtn.width - 6)
                    text: root.path === "" ? "未选择" : root.path
                    color: Theme.fgDim
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    renderType: root.textRenderType
                }
            }

            Text {
                width: parent.width
                text: root.infoText
                color: root.videoValid ? Theme.fgDim : Theme.red
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: root.textRenderType
            }
        }
    }
}
```

- [ ] **Step 5: 构建 + 静态检查 + 冒烟**

Run:
```bash
cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/node/VideoInputNode.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功；qmllint 无 error；`24/24` 通过（Task 1 的测试不受影响）。

- [ ] **Step 6: 实机冒烟（GUI）**

`./bin/main`：从节点库搜索「视频」→ 添加「视频输入」→ 点「选择视频」选 `assets/test.mkv`：
- 信息行显示 `1744 × 982 · 30.3 FPS · 2863 帧 · 36.0 MiB`（`36 MB` 量级即可）；
- 自动跑一次后卡片出现缩略图（首帧）；
- 换一个不存在的路径（在对话框里手打）→ 信息行变红「文件不存在」或「无法打开」。
截图留证到 `/tmp/opencode/video-task2-*.png`。

- [ ] **Step 7: 汇报并等待用户确认后提交**

用户说「提交」后执行：
```bash
git add include/node/VideoInput.hpp include/FileDialogs.h include/engine/BuiltinExecutors.hpp \
        qml/palette/NodeCatalog.qml qml/palette/PaletteItem.qml assets.qrc src/main.cpp \
        qml/node/VideoInputNode.qml
git commit -m "feat(video): VideoInput node with file picker, video info and registration"
```

---

## Task 3: 播放条（播放/暂停/帧滑块/fps/循环 + 触发运行）

**Files:**
- Modify: `qml/node/VideoInputNode.qml`（在 Task 2 的最小版上加播放条、loop、定时器）

**Interfaces:**
- Consumes: Task 2 的 `VideoInputNode` 属性（`path/fps/loop/frame/frameCount/videoValid/infoText`）、`NodeManager.run()`/`NodeManager.engineRunning()`/`NodeManager.commitNodeParams(uuid)`。
- Produces: 无新接口（纯 QML）。

- [ ] **Step 1: 用完整播放条替换 Task 2 的 QML**

把 `qml/node/VideoInputNode.qml` 整体替换为：

```qml
import QtQuick
import QtQuick.Controls
import VideoInputNode
import Theme
import Settings
import FileDialogs
import NodeManager

VideoInputNode {
    id: root
    width: 240
    height: Settings.autoHeight ? Math.max(root.getMinHeight(), card.contentHeight) : root.getMinHeight()

    readonly property int textRenderType:
        Settings.textRender === "native" ? Text.NativeRendering : Text.CurveRendering
    // 播放状态（纯 UI 态，不进 params）
    property bool playing: false

    NodeCard {
        id: card
        anchors.fill: parent
        node: root
        coordItem: root.parent
        previewSource: "qrc:/preview_placeholder.png"

        Column {
            width: parent ? parent.width : implicitWidth
            spacing: 4

            // ---- 选择文件 ----
            Row {
                width: parent.width
                spacing: 6
                Rectangle {
                    id: pickBtn
                    anchors.verticalCenter: parent.verticalCenter
                    width: pickLabel.implicitWidth + 20
                    height: 24
                    radius: 6
                    color: pickHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: pickHover.hovered ? Theme.blue : Theme.border
                    Text {
                        id: pickLabel
                        anchors.centerIn: parent
                        text: "选择视频"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: root.textRenderType
                    }
                    HoverHandler { id: pickHover }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            const p = FileDialogs.openVideo(root.path)
                            if (p !== "") {
                                root.path = p
                                NodeManager.commitNodeParams(root.uuid)
                                NodeManager.run()
                            }
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - pickBtn.width - 6)
                    text: root.path === "" ? "未选择" : root.path
                    color: Theme.fgDim
                    font.pixelSize: 10
                    elide: Text.ElideMiddle
                    renderType: root.textRenderType
                }
            }

            // ---- 信息行 ----
            Text {
                width: parent.width
                text: root.infoText
                color: root.videoValid ? Theme.fgDim : Theme.red
                font.pixelSize: 10
                elide: Text.ElideRight
                renderType: root.textRenderType
            }

            // ---- 播放条 ----
            Row {
                width: parent.width
                spacing: 6

                Rectangle {
                    id: playBtn
                    width: 30
                    height: 24
                    radius: 6
                    enabled: root.videoValid
                    opacity: enabled ? 1 : 0.4
                    color: playHover.hovered ? Theme.bgHover : Theme.bgElev
                    border.width: 1
                    border.color: playHover.hovered ? Theme.blue : Theme.border
                    Text {
                        anchors.centerIn: parent
                        text: root.playing ? "⏸" : "▶"
                        color: Theme.fg
                        font.pixelSize: 12
                        renderType: root.textRenderType
                    }
                    HoverHandler { id: playHover }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.playing = !root.playing
                    }
                }

                Slider {
                    id: frameSlider
                    width: Math.max(50, parent.width - playBtn.width - frameText.width
                                    - fpsBox.width - 4 * parent.spacing)
                    enabled: root.frameCount > 0
                    from: 0
                    to: Math.max(0, root.frameCount - 1)
                    value: root.frame
                    onMoved: {
                        root.playing = false
                        root.frame = Math.round(value)
                        NodeManager.run()
                    }
                }

                Text {
                    id: frameText
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.frameCount > 0
                          ? (root.frame + 1) + " / " + root.frameCount
                          : "" + (root.frame + 1)
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: root.textRenderType
                }

                Rectangle {
                    id: fpsBox
                    width: 38
                    height: 22
                    radius: 5
                    color: Theme.bg
                    border.width: 1
                    border.color: fpsIn.activeFocus ? Theme.blue : Theme.border
                    TextInput {
                        id: fpsIn
                        anchors.fill: parent
                        horizontalAlignment: TextInput.AlignHCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.fg
                        font.pixelSize: 11
                        selectByMouse: true
                        renderType: root.textRenderType
                        text: "" + root.fps
                        onEditingFinished: {
                            const v = parseInt(text, 10)
                            if (isNaN(v)) {
                                text = "" + root.fps      // 非法输入回显当前值
                            } else {
                                root.fps = v
                                NodeManager.commitNodeParams(root.uuid)
                            }
                        }
                    }
                }
            }

            // ---- 循环开关 ----
            Row {
                width: parent.width
                spacing: 6
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "循环播放"
                    color: Theme.fgDim
                    font.pixelSize: 10
                    renderType: root.textRenderType
                }
                Rectangle {
                    width: 30
                    height: 17
                    radius: 8.5
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.loop ? Theme.blue : Theme.bg
                    border.width: 1
                    border.color: root.loop ? Theme.blue : Theme.border
                    Rectangle {
                        width: 13
                        height: 13
                        radius: 6.5
                        y: 2
                        x: root.loop ? 15 : 2
                        color: root.loop ? "#ffffff" : Theme.fgDim
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.loop = !root.loop
                            NodeManager.commitNodeParams(root.uuid)
                        }
                    }
                }
            }
        }
    }

    // 播放定时器：每帧推进并触发一次全图运行；上一帧还没跑完就跳过（不堆积）
    Timer {
        id: playTimer
        interval: Math.max(1, 1000 / Math.max(1, root.fps))
        repeat: true
        running: root.playing && root.videoValid
        onTriggered: {
            if (NodeManager.engineRunning()) return
            const total = root.frameCount
            if (total > 0 && root.frame + 1 >= total) {
                if (root.loop) {
                    root.frame = 0
                } else {
                    root.playing = false
                    return
                }
            } else {
                root.frame = root.frame + 1
            }
            NodeManager.run()
        }
    }
}
```

- [ ] **Step 2: 构建 + 静态检查**

Run:
```bash
cmake --build build -j4
/usr/lib/qt6/bin/qmllint qml/node/VideoInputNode.qml
ctest --test-dir build --output-on-failure
```
Expected: 构建成功；qmllint 无 error（既有的 import/unqualified 类提示可忽略）；`24/24`。

- [ ] **Step 3: 实机验证播放交互**

`./bin/main`，添加「视频输入」→ 选 `assets/test.mkv`：
1. 点 ▶ → 队列芯片逐帧滚动、缩略图随帧变化、`帧号 / 总帧数` 在走；
2. 点 ⏸ → 立即停；
3. 拖帧滑块到中间 → 立即出该帧（拖动时自动暂停）；
4. fps 改成 `5` → 播放明显变慢；
5. `loop` 关 → 播到尾帧自动停（`⏸` 回到 `▶`）；
6. `loop` 开 → 到尾帧回绕到第 1 帧继续；
7. 播放中改 fps / 切换 loop → 不崩溃、下一帧起生效。
截图留证到 `/tmp/opencode/video-task3-*.png`。

- [ ] **Step 4: 验证撤销栈语义**

1. 改 fps → `Ctrl+Z` → fps 恢复（进栈 ✓）；
2. 播放 10 帧后 `Ctrl+Z` → **只撤销最近一次用户操作**（fps/选文件），不会逐帧回退（帧号不进栈 ✓）；
3. 保存（`Ctrl+S`）→ 重开 → `path/fps/loop/frame` 恢复（从保存时的帧继续）。

- [ ] **Step 5: 汇报并等待用户确认后提交**

用户说「提交」后执行：
```bash
git add qml/node/VideoInputNode.qml
git commit -m "feat(video): in-node playback bar (play/pause, frame slider, fps, loop)"
```

---

## Task 4: 端到端链路验证与全量回归

**Files:** 无代码改动（只验证；发现问题回到对应 Task 修）。

- [ ] **Step 1: 搭链路并播放**

`./bin/main`，搭：`视频输入(assets/test.mkv) → 预处理(标准预处理, 输出尺寸=指定大小, 宽高=640×640) → ONNX 推理(models/yolo11n.onnx, 设备 自动) → 后处理(YOLO 检测)`，运行一次后点播放：
- 队列逐帧「完成 4/4」，耗时芯片可见（加载图片/预处理/ONNX/后处理）；
- 后处理输出图像有检测框（占位图上可能框少，属正常；重点是**不报错、逐帧出图**）；
- 观察单帧总耗时应与单张图片链路同量级（无累积变慢）。
截图 `/tmp/opencode/video-task4-chain.png`。

- [ ] **Step 2: 异常路径人工核对（Review Focus）**

1. 播放中把视频路径改到一个不存在的文件 → 节点红点 + 悬停错误 + 队列失败芯片，播放停止，不崩溃；
2. 播放中删除/移动 `assets/test.mkv`（或改路径后改回）→ 缓存能重建、错误可恢复；
3. 手动把帧号输入到很大值（若有入口）或拖滑块到末端 + loop 关 → 尾帧后停止；
4. 播放中点顶栏取消/或切到别的图 → 定时器停止、无残留运行；
5. 保存图 → 关掉 → 把 `assets/test.mkv` 改名 → 重开图 → 明确报错且不崩溃。

- [ ] **Step 3: 全量测试与静态检查**

Run:
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
/usr/lib/qt6/bin/qmllint qml/node/VideoInputNode.qml qml/node/ImageLoadNode.qml
```
Expected: `24/24` 通过；qmllint 无 error。

- [ ] **Step 4: 汇报验收结果**

向用户汇报：链路截图、异常路径结果、测试结果；如有问题列出复现步骤与修复建议，等待用户决定。

---

## 备注

- `cv::VideoCapture` 的 `CAP_PROP_FRAME_COUNT` 对某些容器可能返回 0（未知）：此时滑块禁用、`帧号 / 总帧数` 只显示帧号，播放到读失败即停（由执行器的「读取第 N 帧失败」报错体现）。
- 执行器的 capture 缓存是进程级静态表：同一路径只开一次；路径被删后下次调用会重建并报错。测试里用了不存在的路径，不会污染缓存（打不开的条目不缓存）。
- 摄像头/RTSP 不在本计划范围（spec 非目标）；`source` 参数预留，实现时不要提前加入。
