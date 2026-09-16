#include "NodeManager.h"
#include "PaintBoard.h"
#include "Theme.h"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QUrl>

int main(int argc, char *argv[]){
    // 高 DPI 下按真实缩放因子渲染，避免文字被合成器二次缩放而发虚
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    const QUrl url("qrc:/main.qml");
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl) {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);
    qmlRegisterType<PaintBoard>("PaintBoard", 1, 0, "PaintBoard");
    qmlRegisterType<ImageLoadNode>("ImageLoadNode", 1, 0, "ImageLoadNode");
    qmlRegisterType<ImageShowNode>("ImageShowNode", 1, 0, "ImageShowNode");
    qmlRegisterSingletonInstance("NodeManager", 1, 0, "NodeManager", NodeManager::instance());
    qmlRegisterSingletonInstance("Theme", 1, 0, "Theme", Theme::instance());
    qmlRegisterSingletonType(QUrl("qrc:/NodeCatalog.qml"), "NodeCatalog", 1, 0, "NodeCatalog");
    qmlRegisterSingletonType(QUrl("qrc:/UiBus.qml"), "UiBus", 1, 0, "UiBus");
    engine.load(url);
    return app.exec();
}
