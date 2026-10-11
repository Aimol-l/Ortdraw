# Ortdraw 视频输入节点（单帧 + 节点内播放条）— 设计文档

- 日期：2026-10-11
- 状态：待评审
- 范围：新增「视频输入」节点：从本地视频文件取**一帧**输出为 `Image`，卡片内带播放条（播放/暂停、帧滑块 seek、fps、循环），播放时按 fps 推进帧号并触发一次全图运行，从而把视频逐帧送入现有链路（传统算子 / 预处理 → ONNX → 后处理）。
- 非目标：摄像头 / RTSP / 屏幕采集（`source` 参数预留，另行扩展）；把结果写回视频文件；音轨、字幕、变速、精确到关键帧的 seek 优化；多视频节点同步或全局播放控制；引擎改为流式/持续求值。

## 1. 目标

1. 提供 `视频输入` 节点（`input` 类）：选择本地视频文件后，按帧号输出一帧 BGR 图像，端口类型 `Image`，可直接连接任何吃 `Image` 的节点。
2. 卡片内播放条：`选择视频` 按钮 + 信息行 + 播放/暂停 + 帧滑块（可拖拽 seek）+ `帧号 / 总帧数` + fps；播放时按设定帧率推进并触发一次运行。
3. 完整复用现有机制：运行（`NodeManager.run()`）、执行队列/耗时/取消、节点错误通道（红点 + 悬停提示 + 失败芯片）、`ImageStore` 缩略图预览、`.ortdraw` 存取与撤销栈。**引擎零改动**。
4. 失败有明确分类提示：文件不存在 / 打不开（缺解码器）/ 帧号越界 / 读取失败。
5. 测试素材：`assets/test.mkv`（h264、1744×982、30fps、约 36MB，已存在于工作区，**加入 `.gitignore`**，不入库）。
6. 可行性探针（2026-10-11，一次性程序，已删除）：`cv::VideoCapture` 可直接打开该文件，`WIDTH=1744 / HEIGHT=982 / FPS=30.303 / FRAME_COUNT=2863`；`CAP_PROP_POS_FRAMES` seek 到 0/30/900 均成功，读出 `1744×982`、`type=64`（`CV_8UC3`，BGR，与 `cv::imread` 一致）。

## 2. 现状与约束

- 节点范式（照抄 `ImageLoad`）：C++ `BaseNode` 子类（`typeName/category/params/setParams` + QML 卡片）+ `NodeExecutor` 子类 + `NodeCatalog`/`assets.qrc`/`main.cpp` 注册 + `FileDialogs` 选文件。
- 执行引擎：一次 `run()` = 后台线程按拓扑序跑一遍，`NodeExecutor::execute(ctx, params, inputs) -> ExecResult{ok,error,outputs}`；`NodeData = variant<monostate, cv::Mat, Tensor, double, bool>`；端口类型 `Image/Tensor/Number/Bool/Any`。
- 求值在**单线程**拓扑序中进行，故执行器内的缓存只需普通 mutex。
- OpenCV 5.0 且 `libopencv_videoio.so` 在位 → 使用 `cv::VideoCapture`（本地文件；摄像头/RTSP 后续）。
- 撤销栈：`NodeManager.commitNodeParams(uuid)` 把当前 `params()` 压入快照。播放时每帧都推进帧号，**不能**每次都压栈。

## 3. 节点设计

### 3.1 C++ 节点 `include/node/VideoInput.hpp`

```
class VideoInputNode : public BaseNode
```
- 属性（Q_PROPERTY，均带 NOTIFY）：
  - `path`（QString）：视频路径；`setPath` 时清缓存并刷新信息。
  - `fps`（int，默认 25，范围 1–120）：播放帧率。
  - `loop`（bool，默认 true）：播到尾是否循环。
  - `frame`（int，默认 0）：当前帧号；QML 播放时直接赋值（**不进撤销栈**）。
  - `infoText`（QString，只读）：`W × H · FPS · N 帧 · 文件大小`；打不开时为错误说明。
  - `frameCount`（int，只读）：总帧数（`CAP_PROP_FRAME_COUNT`，未知时为 0）。
  - `videoValid`（bool，只读）：能否打开（用于启用/禁用播放条）。
- `params()` 返回 `{path, fps, loop, frame}`；`setParams()` 恢复四者（`frame` 缺省 0）。
- `videoInfo()`：打开一次 `VideoCapture` 读 `WIDTH/HEIGHT/FPS/FRAME_COUNT`，**不持有** capture（执行器自己缓存）；结果按 `path` 缓存，避免每次重开。
- 端口：单输出 `Port("输出", Output, DataType::Image)`；`min_width = 240`（播放条比图片节点宽）。

### 3.2 执行器 `include/engine/executors/VideoInputExecutor.hpp`

- 取 `path/frame`；`path` 为空 → `{false, "未选择视频文件", {}}`。
- 按 `path` 缓存 `cv::VideoCapture`（`static` map + mutex；`path` 变化或 `reload` 时重建）。摄像头源后续用 `source` 参数区分，本版仅文件。
- `if (!cap.isOpened())` → `{false, "无法打开视频（缺解码器？）: path", {}}`。
- 帧号归一化：`total = CAP_PROP_FRAME_COUNT`；`frame < 0 || (total > 0 && frame >= total)` → `{false, "帧号超出范围（共 N 帧）", {}}`（QML 侧循环模式会先取模，故此错误主要来自手动输入）。
- `cap.set(CAP_PROP_POS_FRAMES, frame)` → `cap.read(mat)`；`mat.empty()` → `{false, "读取帧失败", {}}`。
- 输出 `mat`（已是 BGR 8UC3，与 `cv::imread` 一致；下游无需改动）。

### 3.3 文件对话框 `include/FileDialogs.h`

- 新增 `Q_INVOKABLE QString openVideo(startDir)`：过滤器 `视频 (*.mp4 *.mov *.avi *.mkv *.webm *.flv *.ts *.m4v)`，默认目录 `~/Videos`（不存在退回主目录），与 `openImage` 同风格。

### 3.4 QML `qml/node/VideoInputNode.qml`

- `NodeCard` + `Column`：
  1. 行 1：`选择视频` 按钮（`FileDialogs.openVideo`）+ 路径省略显示。
  2. 信息行：`infoText`（10px 灰字，省略）。
  3. 播放条行：播放/暂停按钮（▶/⏸，`Theme.blue` 描边）+ 帧滑块（`Slider`，`from 0 to max(0, frameCount-1)`，`frameCount == 0` 时禁用；`onMoved` 暂停并设 `frame`）+ `帧号 / 总帧数` 文本 + fps 输入（小输入框）。
  4. `loop` 开关（标签 + 开关，仿 `ParamField` 的 bool 样式）。**更换文件时 `frame` 归零**并刷新信息/总帧数。
- 播放定时器：`Timer { interval: 1000 / fps; repeat: true; running: playing }` → `onTriggered`：
  - `frame + 1 >= frameCount`：`loop ? frame = 0 : (playing = false; return)`；
  - 否则 `frame = frame + 1`；
  - 每次改帧后调用 `NodeManager.run()`（与顶栏「运行」同一入口；运行中队列正常显示，可用现有取消）。
- 拖拽滑块 / 改帧号时同样触发一次 `run()`。
- 用户显式操作（选文件、改 fps、改 loop）走 `NodeManager.commitNodeParams(root.uuid)` 进撤销栈；**帧号一律不进栈**（播放每帧推进、拖滑块连续移动都会刷爆撤销栈，故统一只把 path/fps/loop 作为可撤销参数）。
- 选中文件后自动跑一次（让 `ImageStore` 出缩略图；`previewSource` 保持占位图，不把视频当图片源）。

### 3.5 注册

- `assets.qrc`：加 `VideoInputNode.qml`。
- `src/main.cpp`：`qmlRegisterType<VideoInputNode>("VideoInputNode", 1, 0, "VideoInputNode")`。
- `NodeCatalog`：`input` 类、名称「视频输入」、描述「读取视频文件的一帧，输出图像」、palette 关键词「视频 video 摄像头 帧」。
- `BuiltinExecutors.hpp`：`r.registerExecutor("VideoInput", ...)`。

## 4. 数据流

```
VideoInputNode(播放/seek) --Image--> [传统算子...] / PreProcess --> OnnxInfer --> PostProcess
        │                                   （每改一次 frame 触发一次 NodeManager.run()）
        └-- 失败 --> 节点错误通道（红点/悬停/失败芯片）
```

- 一次运行 = 一帧；帧与帧之间完全独立（无状态泄漏），取消/失败语义与现状一致。
- 大图（1744×982）→ 预处理「指定 640×640」→ ONNX，验证缩放链路与耗时（队列芯片可见）。

## 5. 错误处理

| 情况 | 行为 |
|---|---|
| 未选文件 | 执行器报错「未选择视频文件」；播放条禁用 |
| 文件不存在 / 打不开 / 缺解码器 | 执行器报错「无法打开视频（缺解码器？）: 路径」；`infoText` 显示同样信息 |
| 帧号越界（手动输入） | 执行器报错「帧号超出范围（共 N 帧）」 |
| 读帧失败 | 执行器报错「读取帧失败」 |
| `frameCount` 未知（0） | 滑块禁用、只显示帧号；播放按「读失败即停」处理 |

## 6. 持久化与兼容

- `.ortdraw`：`{type:"VideoInput", x,y,w,h, params:{path,fps,loop,frame}}`；加载后从 `frame` 开始（未保存则为 0）。
- 撤销/重做：仅「选文件 / fps / loop」进栈；帧号（播放推进与手动 seek）一律不进栈。
- 旧图不受影响（新节点类型，老图没有它）。
- `assets/test.mkv` 加入 `.gitignore`。

## 7. 测试步骤

1. 构建：`cmake --build build -j4`；`/usr/lib/qt6/bin/qmllint qml/node/VideoInputNode.qml`；`ctest --test-dir build --output-on-failure`（含新增 `test_video_input`）。
2. 单测（`tests/test_video_input.cpp`）：
   - 空 `path` → 执行器失败且错误信息含「未选择视频」；
   - 不存在的文件 → 失败且含「无法打开」；
   - `assets/test.mkv` 存在时：`frame=0` 与 `frame=30` 均成功且输出为 `1744×982` 的 8UC3；`frame=999999` → 失败且含「帧号超出范围」；
   - `frameCount`/`fps` 读取正确（>0）。
   - 若 `assets/test.mkv` 不存在则 `QSKIP`（该文件不入库，本机/开发者自备）。
3. 实机（`./bin/main`）：
   - 添加「视频输入」→ 选 `assets/test.mkv` → 信息行显示 `1744 × 982 · 30 FPS · N 帧 · 36 MB`；自动跑一次后卡片出现缩略图；
   - 播放：队列芯片逐帧滚动、状态栏「完成 N/N」、缩略图随帧变化；暂停后停止；
   - 拖帧滑块到中间 → 立即出该帧；`loop` 关时播到尾自动停；
   - 改 fps（如 5）→ 播放变慢；
   - 链路：`视频输入 → 预处理(指定 640×640) → ONNX(yolo11n) → 后处理(检测)`，播放时逐帧出框，输出图像尺寸=原帧；
   - 错误：把路径改到不存在的文件 → 节点红点 + 悬停提示 + 队列失败芯片；
   - 保存 `.ortdraw` → 重开 → 路径/fps/loop/帧号恢复；
   - 撤销：选文件/改 fps/loop 可撤销；帧号变化（播放与手动 seek）不产生撤销条目。

## 8. 验收标准

- 视频输入节点可加载 `assets/test.mkv`，输出帧与 `cv::imread` 同语义（BGR 8UC3），尺寸/帧率信息正确。
- 播放条可播放/暂停/seek/调 fps/循环；播放期间队列、耗时、取消、错误提示均正常，无界面卡死（每帧一次全图重跑，单帧耗时为原有链路耗时）。
- 引擎、既有节点、既有图的保存/加载行为无回归（`ctest` 全绿 + 上述人工项通过）。
- 测试素材不入库（`.gitignore` 已加）。

## 9. 变更记录

| 日期       | 版本  | 描述                                   | 作者   |
|------------|-------|----------------------------------------|--------|
| 2026-10-11 | 0.1.0 | 初稿：视频输入节点（单帧 + 节点内播放条） | opencode |
