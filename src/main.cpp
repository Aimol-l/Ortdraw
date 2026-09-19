#include "NodeManager.h"
#include "PaintBoard.h"
#include "Settings.h"
#include "Theme.h"
#include "FileDialogs.h"
#include "Log.hpp"
#include "node/ImageLoad.hpp"
#include "node/ImageSave.hpp"
#include "node/ImageShow.hpp"
#include "node/Resize.hpp"
#include "node/Blur.hpp"
#include "node/Threshold.hpp"
#include "node/Gray.hpp"
#include "node/EdgeDetect.hpp"
#include "node/Conv.hpp"
#include "node/Tensor.hpp"
#include "engine/BuiltinExecutors.hpp"
#include "engine/ImageStore.hpp"
#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QUrl>

int main(int argc, char *argv[]){
    // 高 DPI 下按真实缩放因子渲染，避免文字被合成器二次缩放而发虚
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QCoreApplication::setOrganizationName(QStringLiteral("Ortdraw"));
    QCoreApplication::setApplicationName(QStringLiteral("Ortdraw"));
    QApplication app(argc, argv);
    Log::init();
    Log::info(QStringLiteral("应用启动"));
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [] {
        Log::info(QStringLiteral("应用退出"));
    });
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
    qmlRegisterType<ImageSaveNode>("ImageSaveNode", 1, 0, "ImageSaveNode");
    qmlRegisterType<ResizeNode>("ResizeNode", 1, 0, "ResizeNode");
    qmlRegisterType<BlurNode>("BlurNode", 1, 0, "BlurNode");
    qmlRegisterType<ThresholdNode>("ThresholdNode", 1, 0, "ThresholdNode");
    qmlRegisterType<GrayNode>("GrayNode", 1, 0, "GrayNode");
    qmlRegisterType<EdgeDetectNode>("EdgeDetectNode", 1, 0, "EdgeDetectNode");
    qmlRegisterType<ConvNode>("ConvNode", 1, 0, "ConvNode");
    qmlRegisterType<TensorNode>("TensorNode", 1, 0, "TensorNode");
    qmlRegisterSingletonInstance("NodeManager", 1, 0, "NodeManager", NodeManager::instance());
    qmlRegisterSingletonInstance("Settings", 1, 0, "Settings", Settings::instance());
    qmlRegisterSingletonInstance("Theme", 1, 0, "Theme", Theme::instance());
    qmlRegisterSingletonInstance("FileDialogs", 1, 0, "FileDialogs", FileDialogs::instance());
    qmlRegisterSingletonInstance("Log", 1, 0, "Log", Log::instance());
    qmlRegisterSingletonType(QUrl("qrc:/NodeCatalog.qml"), "NodeCatalog", 1, 0, "NodeCatalog");
    qmlRegisterSingletonType(QUrl("qrc:/UiBus.qml"), "UiBus", 1, 0, "UiBus");
    registerBuiltinExecutors();
    engine.addImageProvider("nodeimage", new NodeImageProvider());
    // 放大查看的全分辨率提供者：注入解析回调，惰性向引擎索取并缓存
    auto* nodeManager = static_cast<NodeManager*>(NodeManager::instance());
    engine.addImageProvider("nodeimagefull", new FullImageProvider(
        [nodeManager](const QString& uuid) { return nodeManager->fullImage(uuid); }));
    engine.load(url);
    return app.exec();
}
