#pragma once

#include <QQuickItem>
#include <QVariantList>
#include "node/BaseNode.hpp"

// 张量：输出一个 rows×cols 的 FLOAT32 2D 卷积核，供卷积节点使用
class TensorNode : public BaseNode {
    Q_OBJECT
    Q_PROPERTY(int rows READ rows WRITE setRows NOTIFY paramsChanged)
    Q_PROPERTY(int cols READ cols WRITE setCols NOTIFY paramsChanged)
    Q_PROPERTY(QString shapeText READ shapeText NOTIFY paramsChanged)
public:
    QString typeName() const override { return "Tensor"; }
    QString category() const override { return "math"; }

    int rows() const { return m_rows; }
    int cols() const { return m_cols; }
    QString shapeText() const { return QStringLiteral("%1 × %2").arg(m_rows).arg(m_cols); }

    void setRows(int v) { setShape(v, m_cols); }
    void setCols(int v) { setShape(m_rows, v); }

    Q_INVOKABLE double value(int r, int c) const {
        if (r < 0 || c < 0 || r >= m_rows || c >= m_cols) return 0.0;
        const int idx = r * m_cols + c;
        return (idx >= 0 && idx < m_data.size()) ? m_data[idx] : 0.0;
    }

    Q_INVOKABLE void setValue(int r, int c, double v) {
        if (r < 0 || c < 0 || r >= m_rows || c >= m_cols) return;
        const int idx = r * m_cols + c;
        if (idx < 0 || idx >= m_data.size()) return;
        if (qFuzzyCompare(m_data[idx] + 1.0, v + 1.0)) return;
        m_data[idx] = v;
        emit paramsChanged();
    }

    // 重置为单位核：中心为 1，其余为 0
    Q_INVOKABLE void presetIdentity() {
        m_data = QList<double>(m_rows * m_cols, 0.0);
        if (m_rows > 0 && m_cols > 0)
            m_data[(m_rows / 2) * m_cols + (m_cols / 2)] = 1.0;
        emit paramsChanged();
    }

    // 均值核：所有元素均为 1/(rows*cols)
    Q_INVOKABLE void presetMean() {
        const double v = 1.0 / double(m_rows * m_cols);
        m_data = QList<double>(m_rows * m_cols, v);
        emit paramsChanged();
    }

    // 锐化核：中心 5，上下左右 -1，其余 0；仅 3x3 有效，其余退化为单位核
    Q_INVOKABLE void presetSharpen() {
        if (m_rows != 3 || m_cols != 3) {
            presetIdentity();
            return;
        }
        m_data = { 0.0, -1.0, 0.0, -1.0, 5.0, -1.0, 0.0, -1.0, 0.0 };
        emit paramsChanged();
    }

    // 调整尺寸，保留重叠区域内的值，新增元素补 0
    Q_INVOKABLE void setShape(int rows, int cols) {
        rows = qBound(1, rows, 5);
        cols = qBound(1, cols, 5);
        if (rows == m_rows && cols == m_cols) return;
        QList<double> nd(rows * cols, 0.0);
        for (int r = 0; r < rows && r < m_rows; ++r)
            for (int c = 0; c < cols && c < m_cols; ++c)
                nd[r * cols + c] = m_data[r * m_cols + c];
        m_rows = rows;
        m_cols = cols;
        m_data = nd;
        emit paramsChanged();
    }

    int kernelRows() const { return m_rows; }
    int kernelCols() const { return m_cols; }
    QList<double> kernelData() const { return m_data; }

    QVariantMap params() const override {
        QVariantList dl;
        dl.reserve(m_data.size());
        for (double v : m_data) dl.append(v);
        return { { "rows", m_rows }, { "cols", m_cols }, { "data", dl } };
    }

    void setParams(const QVariantMap& p) override {
        int r = p.contains("rows") ? p.value("rows").toInt() : m_rows;
        int c = p.contains("cols") ? p.value("cols").toInt() : m_cols;
        r = qBound(1, r, 5);
        c = qBound(1, c, 5);
        QList<double> nd(r * c, 0.0);
        if (p.contains("data")) {
            const QVariantList dl = p.value("data").toList();
            for (int i = 0; i < nd.size() && i < dl.size(); ++i)
                nd[i] = dl[i].toDouble();
        }
        m_rows = r;
        m_cols = c;
        m_data = nd;
        emit paramsChanged();
    }

    TensorNode(QQuickItem *parent = nullptr): BaseNode(parent){
        m_name = "张量";

        m_description = "输出 FLOAT32 二维卷积核（供卷积使用）。";
        m_output_ports.push_back(new Port("张量", PortType::Output, DataType::Tensor, QPointF(0,0), this));
        presetIdentity();
    }
    ~TensorNode(){}

signals:
    void paramsChanged();
private:
    int m_rows = 3;
    int m_cols = 3;
    QList<double> m_data;
};
