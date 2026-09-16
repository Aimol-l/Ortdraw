#include <QtTest>
#include "PaintBoard.h"
#include "port/Port.hpp"

class TestPaintBoard : public QObject {
    Q_OBJECT
private slots:
    void startDrawingCreatesTempPort() {
        PaintBoard board;
        Port start("out", PortType::Output, DataType::Image, QPointF(10, 20), nullptr);
        board.startDrawing(&start, QPointF(10, 20));
        QVERIFY(board.m_drawing_line);
        QCOMPARE(board.m_drawing_edge.start_port, &start);
        QVERIFY(board.m_drawing_edge.stop_port != nullptr);
    }
    void moveDrawingUpdatesStopPort() {
        PaintBoard board;
        Port start("out", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        board.startDrawing(&start, QPointF(0, 0));
        board.moveDrawing(QPointF(77, 88));
        QCOMPARE(board.m_drawing_edge.stop_port->position(), QPointF(77, 88));
    }
    void cancelDrawingClearsState() {
        PaintBoard board;
        Port start("out", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        board.startDrawing(&start, QPointF(0, 0));
        board.cancelDrawing();
        QVERIFY(!board.m_drawing_line);
        QVERIFY(board.m_drawing_edge.start_port == nullptr);
        QVERIFY(board.m_drawing_edge.stop_port == nullptr);
    }
    void finishDrawingIsCancel() {
        PaintBoard board;
        Port start("out", PortType::Output, DataType::Image, QPointF(0, 0), nullptr);
        board.startDrawing(&start, QPointF(0, 0));
        board.finishDrawing();
        QVERIFY(!board.m_drawing_line);
        QVERIFY(board.m_drawing_edge.stop_port == nullptr);
    }
};

QTEST_MAIN(TestPaintBoard)
#include "test_paintboard.moc"
