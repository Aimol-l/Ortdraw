#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

#include <QHash>
#include <QImage>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>
#include <QVariantMap>
#include <opencv2/imgproc.hpp>

#include "Log.hpp"
#include "engine/NodeRegistry.hpp"
#include "engine/NodeStatus.hpp"
#include "utils/DAGraph.hpp"

// 在后台线程按拓扑序求值整张图，结果经队列信号回主线程。
// 调用方（GUI 线程）在 run() 内对图做快照，工作线程不直接访问 DAGraph。
class GraphExecutor : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
public:
    // 某输入端口的上游来源：源节点 + 源输出端口索引
    struct Incoming {
        BaseNode* from = nullptr;
        int fromPort = -1;
    };

    // 节点快照：在 GUI 线程读取，工作线程只使用该副本，绝不调用 QObject 方法。
    struct NodeSnapshot {
        BaseNode* node = nullptr;  // 仅作缓存键 / 错误集合键
        QString uuid;
        QString type;
        QVariantMap params;
    };

    explicit GraphExecutor(QObject* parent = nullptr) : QObject(parent) {}

    ~GraphExecutor() override {
        m_cancel.store(true);
        if (m_thread.joinable()) m_thread.join();
    }

    void setGraph(DAGraph* g) { m_graph = g; }

    bool running() const { return m_running; }
    QString status() const { return m_status; }

    // 在调用线程快照图并在后台线程开始求值；已在运行则返回 false。
    Q_INVOKABLE bool run() {
        if (m_running) return false;
        if (m_thread.joinable()) m_thread.join();
        if (!m_graph) return false;

        // 本次运行将重建全分辨率结果缓存，先清空上一轮
        clearFullImages();

        QVector<NodeSnapshot> nodes;
        QHash<BaseNode*, QVector<Incoming>> incoming;
        for (BaseNode* n : m_graph->getAllNodes()) {
            NodeSnapshot s;
            s.node = n;
            s.uuid = n->uuid().toString();
            s.type = n->typeName();
            s.params = n->params();
            nodes.push_back(std::move(s));

            QVector<Incoming> ins;
            QList<Port*>& inPorts = n->getInPorts();
            for (Port* ip : inPorts) {
                Incoming inc;
                for (const Edge& e : m_graph->getAllEdges()) {
                    if (e.stop_port != ip) continue;
                    if (!e.start_port || !e.start_port->father()) break;
                    inc.from = e.start_port->father();
                    inc.fromPort = inc.from->getOutPorts().indexOf(e.start_port);
                    break;
                }
                ins.push_back(inc);
            }
            incoming.insert(n, ins);
        }

        m_cancel.store(false);
        const int runId = ++m_run_id;
        setStatus(QStringLiteral("运行中…"));
        setRunning(true);

        m_thread = std::thread([this, runId, nodes, incoming] {
            worker(runId, nodes, incoming);
        });
        return true;
    }

    Q_INVOKABLE void cancel() { m_cancel.store(true); }

    // 按需返回某节点上一轮运行的全分辨率结果；无缓存时惰性转换并缓存。
    // 线程安全：可能由 QML 图像提供者在渲染线程调用。
    Q_INVOKABLE QImage fullImage(const QString& uuid) {
        QMutexLocker l(&m_full_mutex);
        if (m_full_cache.contains(uuid)) return m_full_cache.value(uuid);
        if (!m_full.contains(uuid)) return {};
        const QImage img = matToQImage(m_full.value(uuid));
        if (!img.isNull()) m_full_cache.insert(uuid, img);
        return img;
    }

    Q_INVOKABLE bool hasFullImage(const QString& uuid) const {
        QMutexLocker l(&m_full_mutex);
        return m_full.contains(uuid) || m_full_cache.contains(uuid);
    }

    void clearFullImages() {
        QMutexLocker l(&m_full_mutex);
        m_full.clear();
        m_full_cache.clear();
    }

signals:
    void runningChanged();
    void statusChanged();
    void nodeStarted(const QString& uuid);
    void nodeFinished(const QString& uuid, int status, const QString& error, int durationMs);
    void nodeImageReady(const QString& uuid, const QImage& image);
    void runFinished(bool ok);

private:
    void setRunning(bool r) {
        if (m_running == r) return;
        m_running = r;
        emit runningChanged();
    }

    void setStatus(const QString& s) {
        if (m_status == s) return;
        m_status = s;
        emit statusChanged();
    }

    // 单通道 → Grayscale8；三通道 BGR → RGB888；四通道 BGRA → RGB888。
    // 结果均深拷贝，与 cv::Mat 生命周期解耦。
    static QImage matToQImage(const cv::Mat& m) {
        if (m.empty()) return {};
        if (m.channels() == 1) {
            cv::Mat c = m.clone();
            return QImage(c.data, c.cols, c.rows, int(c.step),
                          QImage::Format_Grayscale8).copy();
        }
        if (m.channels() == 3) {
            cv::Mat rgb;
            cv::cvtColor(m, rgb, cv::COLOR_BGR2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step),
                          QImage::Format_RGB888).copy();
        }
        if (m.channels() == 4) {
            cv::Mat rgb;
            cv::cvtColor(m, rgb, cv::COLOR_BGRA2RGB);
            return QImage(rgb.data, rgb.cols, rgb.rows, int(rgb.step),
                          QImage::Format_RGB888).copy();
        }
        return {};
    }

    // 生成缩略图：最长边不超过 kThumbnailMax，避免 GUI 端持有/绘制全分辨率大图。
    static QImage makeThumbnail(const cv::Mat& m) {
        const int longest = std::max(m.cols, m.rows);
        if (longest <= kThumbnailMax) return matToQImage(m);
        const double scale = double(kThumbnailMax) / double(longest);
        cv::Mat small;
        cv::resize(m, small, cv::Size(), scale, scale, cv::INTER_AREA);
        return matToQImage(small);
    }

    void worker(int runId, QVector<NodeSnapshot> nodes,
                const QHash<BaseNode*, QVector<Incoming>>& incoming) {
        QHash<BaseNode*, const NodeSnapshot*> snap;
        for (const NodeSnapshot& s : nodes) snap.insert(s.node, &s);

        // Kahn 拓扑排序（仅使用 BaseNode* 作为图结构键，不调用其方法）
        QHash<BaseNode*, int> indegree;
        QHash<BaseNode*, QVector<BaseNode*>> successors;
        for (const NodeSnapshot& s : nodes) indegree.insert(s.node, 0);
        for (const NodeSnapshot& s : nodes) {
            BaseNode* n = s.node;
            for (const Incoming& inc : incoming.value(n)) {
                if (!inc.from || !indegree.contains(inc.from)) continue;
                indegree[n] += 1;
                successors[inc.from].append(n);
            }
        }
        QVector<BaseNode*> queue;
        for (const NodeSnapshot& s : nodes)
            if (indegree.value(s.node) == 0) queue.append(s.node);

        QVector<BaseNode*> order;
        while (!queue.isEmpty()) {
            BaseNode* n = queue.takeFirst();
            order.append(n);
            for (BaseNode* s : successors.value(n))
                if (--indegree[s] == 0) queue.append(s);
        }

        QHash<BaseNode*, QVector<NodeData>> cache;
        QSet<BaseNode*> failed;
        bool allOk = true;
        bool aborted = false;
        const auto startedAt = std::chrono::steady_clock::now();

        auto reportStarted = [this, runId](const QString& uuid) {
            QMetaObject::invokeMethod(this, [this, runId, uuid] {
                if (runId != m_run_id.load()) return;
                emit nodeStarted(uuid);
            }, Qt::QueuedConnection);
        };

        auto reportNode = [this, runId](const QString& uuid, int status,
                                        const QString& err, int ms) {
            // 工作线程内直接写日志（Log 线程安全）
            Log::debug(QStringLiteral("节点完成：%1 status=%2 %3")
                           .arg(uuid).arg(status).arg(ms));
            QMetaObject::invokeMethod(this, [this, runId, uuid, status, err, ms] {
                if (runId != m_run_id.load()) return;
                emit nodeFinished(uuid, status, err, ms);
            }, Qt::QueuedConnection);
        };

        for (BaseNode* n : order) {
            if (runId != m_run_id.load()) return;
            if (m_cancel.load()) { aborted = true; break; }

            const NodeSnapshot& ns = *snap.value(n);
            const QString& uuid = ns.uuid;
            bool upstreamFailed = false;
            QVector<NodeData> inputs;
            for (const Incoming& inc : incoming.value(n)) {
                if (!inc.from) {
                    inputs.push_back(std::monostate{});
                    continue;
                }
                if (failed.contains(inc.from) || !cache.contains(inc.from)) {
                    upstreamFailed = true;
                    inputs.push_back(std::monostate{});
                    continue;
                }
                const QVector<NodeData>& outv = cache.value(inc.from);
                if (inc.fromPort >= 0 && inc.fromPort < outv.size())
                    inputs.push_back(outv[inc.fromPort]);
                else
                    inputs.push_back(std::monostate{});
            }

            if (upstreamFailed) {
                failed.insert(n);
                allOk = false;
                reportNode(uuid, int(NodeStatus::Skipped), QStringLiteral("上游节点失败"), 0);
                continue;
            }

            reportStarted(uuid);
            const auto nodeT0 = std::chrono::steady_clock::now();
            ExecResult r;
            auto exec = NodeRegistry::instance().executorFor(ns.type);
            if (!exec) {
                r.ok = false;
                r.error = QStringLiteral("无执行器");
            } else {
                ExecuteContext ctx{ns.uuid, &m_cancel, [](const QString&) {}};
                r = exec->execute(ctx, ns.params, inputs);
            }
            const int nodeMs = int(std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - nodeT0).count());

            const bool cancelled = m_cancel.load();
            const int status = cancelled ? int(NodeStatus::Cancelled)
                                         : (r.ok ? int(NodeStatus::Ok) : int(NodeStatus::Failed));
            if (!r.ok) {
                failed.insert(n);
                allOk = false;
            }
            if (status == int(NodeStatus::Ok))
                cache.insert(n, r.outputs);
            reportNode(uuid, status, r.error, nodeMs);

            if (cancelled) { aborted = true; break; }

            if (r.ok) {
                for (const NodeData& d : r.outputs) {
                    if (!std::holds_alternative<cv::Mat>(d)) continue;
                    const cv::Mat& m = std::get<cv::Mat>(d);
                    if (m.empty()) continue;
                    // 保留全分辨率结果（cv::Mat 引用计数拷贝，零拷贝共享像素），
                    // 供放大查看时按需惰性转换；缩略图仅下采样后发送给 GUI。
                    {
                        QMutexLocker l(&m_full_mutex);
                        m_full.insert(uuid, m);
                        m_full_cache.remove(uuid);
                    }
                    const QImage img = makeThumbnail(m);
                    if (img.isNull()) continue;
                    QMetaObject::invokeMethod(this, [this, runId, uuid, img] {
                        if (runId != m_run_id.load()) return;
                        emit nodeImageReady(uuid, img);
                    }, Qt::QueuedConnection);
                    break;
                }
            }
        }

        // 拓扑排序未覆盖的节点（存在环）标记失败
        if (!aborted && runId == m_run_id.load()) {
            for (const NodeSnapshot& s : nodes) {
                if (order.contains(s.node)) continue;
                failed.insert(s.node);
                allOk = false;
                reportNode(s.uuid, int(NodeStatus::Failed), QStringLiteral("图中存在环"), 0);
            }
        }

        if (runId != m_run_id.load()) return;

        const bool ok = allOk && !aborted;
        const QString finalStatus = aborted ? QStringLiteral("已取消")
                                            : (ok ? QStringLiteral("完成")
                                                  : QStringLiteral("失败"));
        const qint64 elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                     std::chrono::steady_clock::now() - startedAt).count();
        QMetaObject::invokeMethod(this, [this, runId, ok, finalStatus, elapsedMs] {
            if (runId != m_run_id.load()) return;
            Log::info(QStringLiteral("图求值结束：ok=%1，耗时 %2 ms")
                          .arg(ok ? QStringLiteral("true") : QStringLiteral("false"))
                          .arg(elapsedMs));
            emit runFinished(ok);
            setStatus(finalStatus);
            setRunning(false);
        }, Qt::QueuedConnection);
    }

    static constexpr int kThumbnailMax = 256;

    DAGraph* m_graph = nullptr;
    std::thread m_thread;
    std::atomic_bool m_cancel{false};
    std::atomic_int m_run_id{0};
    bool m_running = false;
    QString m_status = QStringLiteral("就绪");
    // 最近一轮运行的全分辨率结果（工作线程写，fullImage() 读，故加锁）
    mutable QMutex m_full_mutex;
    QHash<QString, cv::Mat> m_full;
    QHash<QString, QImage> m_full_cache;
};
