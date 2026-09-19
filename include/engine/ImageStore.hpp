#pragma once
#include <functional>
#include <utility>

#include <QQuickImageProvider>
#include <QImage>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>

// 节点结果图像（缩略图）的全局缓存，作为 QML 的 image://nodeimage 图像提供者。
// 工作线程经队列信号回到主线程写入；QML 侧用 rev 触发 URL 变化以刷新预览。
class ImageStore : public QQuickImageProvider {
public:
    ImageStore() : QQuickImageProvider(QQuickImageProvider::Image) {}
    static ImageStore* instance() { static ImageStore s; return &s; }

    void setThumbnail(const QString& uuid, const QImage& img) {
        QMutexLocker l(&m_mutex);
        m_images[uuid] = img;
        ++m_rev[uuid];
    }
    void clear() {
        QMutexLocker l(&m_mutex);
        m_images.clear();
        m_rev.clear();
    }
    bool has(const QString& uuid) const {
        QMutexLocker l(&m_mutex);
        return m_images.contains(uuid);
    }
    qint64 rev(const QString& uuid) const {
        QMutexLocker l(&m_mutex);
        return m_rev.value(uuid, 0);
    }
    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        QMutexLocker l(&m_mutex);
        // QML 会对 image URL 做百分号编码（uuid 的花括号变成 %7B/%7D），需解码后查表
        const QString key = QUrl::fromPercentEncoding(id.section('?', 0, 0).toUtf8());
        QImage img = m_images.value(key);
        if (size) *size = img.size();
        return img;
    }
private:
    mutable QMutex m_mutex;
    QHash<QString, QImage> m_images;
    QHash<QString, qint64> m_rev;
};

// 交给 QQmlEngine 持有并析构的转发提供者。QQmlEngine::addImageProvider 取得
// 传入对象的所有权，因此不能直接把静态单例注册进去（否则单例的静态析构会二次释放）。
class NodeImageProvider : public QQuickImageProvider {
public:
    NodeImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        return ImageStore::instance()->requestImage(id, size, requested);
    }
};

// 放大查看用的全分辨率图像提供者（image://nodeimagefull/<uuid>?v=rev）。
// 不直接依赖 NodeManager，而是在启动时注入一个解析回调，避免循环依赖；
// 结果在引擎侧按需惰性转换并缓存，无可用图像时返回空 QImage。
class FullImageProvider : public QQuickImageProvider {
public:
    using Resolver = std::function<QImage(const QString&)>;
    explicit FullImageProvider(Resolver resolver)
        : QQuickImageProvider(QQuickImageProvider::Image),
          m_resolver(std::move(resolver)) {}

    QImage requestImage(const QString& id, QSize* size, const QSize&) override {
        const QString key = QUrl::fromPercentEncoding(id.section('?', 0, 0).toUtf8());
        const QImage img = m_resolver ? m_resolver(key) : QImage();
        if (size) *size = img.size();
        return img;
    }

private:
    Resolver m_resolver;
};
