#include "NodeManager.h"
#include "PaintBoard.h"
#include "node/ImageLoad.hpp"
#include "node/ImageShow.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[]){
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
    engine.load(url);
    return app.exec();
}
