
#pragma once

#include <QQuickItem>
#include <QImageReader>
#include <QFileInfo>
#include <QLocale>
#include "node/BaseNode.hpp"


// 将图片读取到这个节点中，这个节点只向后输出image
class ImageLoadNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QString infoText READ infoText NOTIFY infoChanged)
public:
    QString typeName() const override { return "ImageLoad"; }
    QString category() const override { return "input"; }

    QString path() const { return m_path; }
    void setPath(const QString& p) {
        if(m_path == p) return;
        m_path = p;
        m_infoText = computeInfo(p);
        emit pathChanged();
        emit infoChanged();
    }

    // 图片信息：分辨率 · 格式 · 文件大小
    QString infoText() const { return m_infoText; }
    static QString computeInfo(const QString& p) {
        if(p.isEmpty()) return QString();
        const QFileInfo fi(p);
        if(!fi.exists() || !fi.isFile()) return QStringLiteral("文件不存在");
        QImageReader reader(p);
        const QSize sz = reader.size();
        QString fmt = QString::fromLatin1(reader.format()).toUpper();
        if(fmt.isEmpty()) fmt = fi.suffix().toUpper();
        const QString sizeText = QLocale().formattedDataSize(fi.size());
        if(sz.isValid())
            return QStringLiteral("%1 × %2  ·  %3  ·  %4")
                       .arg(sz.width()).arg(sz.height()).arg(fmt).arg(sizeText);
        return QStringLiteral("%1  ·  %2").arg(fmt, sizeText);
    }

    QVariantMap params() const override { return {{ "path", m_path }}; }
    void setParams(const QVariantMap& p) override {
        if(p.contains("path")) setPath(p.value("path").toString());
    }

    ImageLoadNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "加载图片";

        m_description = "从磁盘读取图片，输出图像，并显示分辨率/格式/大小。";
        m_output_ports.push_back(new Port("输出", PortType::Output, DataType::Image, QPointF(0,0), this));
    }
    ~ImageLoadNode(){

    }

signals:
    void pathChanged();
    void infoChanged();
private:
    QString m_path;
    QString m_infoText;
};
