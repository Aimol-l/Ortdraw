#include <QtTest>
#include "onnx_engine/runtime.hpp"
using namespace onnx_engine;

class TestOnnxEngine : public QObject {
    Q_OBJECT
private slots:
    void linksAndProbes() {
        (void)Runtime::cudaAvailable();                     // 必须能链接到库符号
        QCOMPARE(QString::fromLatin1(elementTypeName(ElementType::Float32)), QString("float32"));
        QCOMPARE(elementTypeSize(ElementType::Float32), 4);
    }
};
QTEST_MAIN(TestOnnxEngine)
#include "test_onnx_engine.moc"
