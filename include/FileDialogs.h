#pragma once
#include <QObject>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QtQmlIntegration/qqmlintegration.h>

class FileDialogs : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit FileDialogs(QObject* parent = nullptr) : QObject(parent) {}
    static QObject* instance() { static FileDialogs f; return &f; }

    // 返回选中的本地路径；取消返回空串
    Q_INVOKABLE QString openGraph(const QString& startDir = QString()) {
        return QFileDialog::getOpenFileName(nullptr, tr("打开节点图"),
            startDir, QStringLiteral("Ortdraw 图 (*.ortdraw);;所有文件 (*)"));
    }
    Q_INVOKABLE QString openImage(const QString& startDir = QString()) {
        return QFileDialog::getOpenFileName(nullptr, tr("选择图片"), startDir,
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;所有文件 (*)"));
    }
    Q_INVOKABLE QString saveGraph(const QString& startDir = QString(),
                                  const QString& suggested = QStringLiteral("graph.ortdraw")) {
        const QString dir = startDir.isEmpty()
            ? QDir::homePath() + "/" + suggested
            : startDir;
        QString f = QFileDialog::getSaveFileName(nullptr, tr("保存节点图"),
            dir, QStringLiteral("Ortdraw 图 (*.ortdraw);;所有文件 (*)"));
        if(!f.isEmpty() && !f.endsWith(QStringLiteral(".ortdraw"), Qt::CaseInsensitive))
            f += QStringLiteral(".ortdraw");
        return f;
    }
};
