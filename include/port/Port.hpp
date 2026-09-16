#pragma once
#include <QObject>
#include <QString>
#include <QUuid>
#include <QPointF>
enum class PortType{
    Input,
    Output
};
enum class DataType{
    Image,
    Float,
    Int,
    Bool
};
// using NodeData = 
//     std::variant<int16_t,int32_t,int64_t,float,std::string,bool,
//                 QImage,cv::Mat,std::vector<char>,std::vector<float>
//                 >; 


class BaseNode;
class Port : public QObject{
    Q_OBJECT
public:
    Q_PROPERTY(int type READ type CONSTANT)
    Q_PROPERTY(QUuid uuid READ uuid CONSTANT)
    Q_PROPERTY(Port* self READ self CONSTANT)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged) 
    Q_PROPERTY(QPointF position READ position WRITE setPosition)
private:
    QUuid m_uid;
    QString m_name;
    PortType m_type;
    DataType m_data_type;
    QPointF m_position;
    BaseNode* m_father= nullptr;
    bool m_connected = false;
public:
    explicit Port( QObject *parent = nullptr): QObject(parent){}
    explicit Port(QString name, PortType type, DataType data_type, QPointF position,BaseNode* father, QObject *parent = nullptr)
            : QObject(parent),
                m_name(name),
                m_type(type),
                m_data_type(data_type),
                m_position(position),
                m_father(father)
    {
        m_uid = QUuid::createUuid();
    }

    ~Port(){};

    QUuid uuid() const { return m_uid; }
    Port* self() const { return const_cast<Port*>(this);}
    QString name() const { return m_name; }
    int type() const { return static_cast<int>(m_type); }
    DataType dataType() const { return m_data_type;}
    QPointF position() const { return m_position; }
    BaseNode* father() const { return m_father; }
    bool isConnected() const { return m_connected; }
    void setConnected(bool connected) { m_connected = connected; }
    void setPosition(QPointF pos){
        this->m_position = pos;
    }
    void movedeltaPos(QPointF dpos){
        this->m_position += dpos;
    }
signals:
    void nameChanged();
};