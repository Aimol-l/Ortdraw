#pragma once
#include <QQuickImageProvider>
#include <QImage>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QUrl>

// 节点结果图像的全局缓存，作为 QML 的 image://nodeimage 图像提供者。
// 工作线程经队列信号回到主线程写入；QML 侧用 rev 触发 URL 变化以刷新预览。
class ImageStore : public QQuickImageProvider {
public:
    ImageStore() : QQuickImageProvider(QQuickImageProvider::Image) {}
    static ImageStore* instance() { static ImageStore s; return &s; }

    void setImage(const QString& uuid, const QImage& img) {
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
