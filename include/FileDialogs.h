#pragma once
#include <QObject>
#include <QFileDialog>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QtQmlIntegration/qqmlintegration.h>

class FileDialogs : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    explicit FileDialogs(QObject* parent = nullptr) : QObject(parent) {}
    static QObject* instance() { static FileDialogs f; return &f; }

    // 返回选中的本地路径；取消返回空串
    // 默认目录：~/Pictures（不存在时退回主目录）
    static QString imageStartDir(const QString& startDir) {
        if(!startDir.isEmpty()) return startDir;
        QString pics = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if(pics.isEmpty() || !QDir(pics).exists()) pics = QDir::homePath();
        return pics;
    }
    Q_INVOKABLE QString openGraph(const QString& startDir = QString()) {
        return QFileDialog::getOpenFileName(nullptr, tr("打开节点图"),
            startDir, QStringLiteral("Ortdraw 图 (*.ortdraw);;所有文件 (*)"));
    }
    Q_INVOKABLE QString openImage(const QString& startDir = QString()) {
        return QFileDialog::getOpenFileName(nullptr, tr("选择图片"), imageStartDir(startDir),
            QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp *.tif *.tiff);;所有文件 (*)"));
    }
    Q_INVOKABLE QString openModel(const QString& startPath = QString()) {
        const QString start = startPath.isEmpty() ? QDir::homePath() : startPath;
        return QFileDialog::getOpenFileName(nullptr, tr("选择 ONNX 模型"), start,
            QStringLiteral("ONNX 模型 (*.onnx);;所有文件 (*)"));
    }
    Q_INVOKABLE QString openClassFile(const QString& startPath = QString()) {
        const QString start = startPath.isEmpty() ? QDir::homePath() : startPath;
        return QFileDialog::getOpenFileName(nullptr, tr("选择类别文件"), start,
            QStringLiteral("类别文件 (*.txt);;所有文件 (*)"));
    }
    Q_INVOKABLE QString saveImage(const QString& startPath = QString()) {
        QString f = QFileDialog::getSaveFileName(nullptr, tr("保存图片"),
            startPath.isEmpty() ? QStringLiteral("output.png") : startPath,
            QStringLiteral("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;TIFF (*.tif *.tiff);;WebP (*.webp);;所有文件 (*)"));
        if (!f.isEmpty() && QFileInfo(f).suffix().isEmpty()) f += ".png";
        return f;
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
