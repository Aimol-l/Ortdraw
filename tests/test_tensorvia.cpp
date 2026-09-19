#include <QtTest>
#include <tensorvia/core/tensor.h>

class TestTensorvia : public QObject {
    Q_OBJECT
private slots:
    void createAndRead() {
        Tensor t = Tensor::Zeros({2, 3}, via::DataType::FLOAT32);
        QCOMPARE(int(t.shape(0)), 2);
        QCOMPARE(int(t.shape(1)), 3);
        QCOMPARE(int(t.numel()), 6);
        QCOMPARE(t.dtype(), via::DataType::FLOAT32);
    }
    void fromVector() {
        std::vector<float> v{1.f, 2.f, 3.f, 4.f, 5.f, 6.f};
        std::vector<int64_t> shape{2, 3};
        Tensor t(v, shape);
        QCOMPARE(int(t.numel()), 6);
        QCOMPARE(int(t.shape(0)), 2);
        QCOMPARE(int(t.shape(1)), 3);
    }
};

QTEST_MAIN(TestTensorvia)
#include "test_tensorvia.moc"
