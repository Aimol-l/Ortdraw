#include "NodeManager.h"
#include "PaintBoard.h"
#include "Settings.h"
#include "Theme.h"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include "node/Resize.hpp"
#include "node/Blur.hpp"
#include "node/Threshold.hpp"
#include "node/Conv.hpp"
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
    qmlRegisterType<ResizeNode>("ResizeNode", 1, 0, "ResizeNode");
    qmlRegisterType<BlurNode>("BlurNode", 1, 0, "BlurNode");
    qmlRegisterType<ThresholdNode>("ThresholdNode", 1, 0, "ThresholdNode");
    qmlRegisterType<ConvNode>("ConvNode", 1, 0, "ConvNode");
    qmlRegisterSingletonInstance("NodeManager", 1, 0, "NodeManager", NodeManager::instance());
    qmlRegisterSingletonInstance("Settings", 1, 0, "Settings", Settings::instance());
    qmlRegisterSingletonInstance("Theme", 1, 0, "Theme", Theme::instance());
    qmlRegisterSingletonType(QUrl("qrc:/NodeCatalog.qml"), "NodeCatalog", 1, 0, "NodeCatalog");
    qmlRegisterSingletonType(QUrl("qrc:/UiBus.qml"), "UiBus", 1, 0, "UiBus");
    engine.load(url);
    return app.exec();
}
