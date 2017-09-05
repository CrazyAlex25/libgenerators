#ifndef SERVER_H
#define SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(int port, QObject *parent = nullptr);
    bool start(int port);
    bool start();

signals:
    void connected();
    void disconnected();
    void error(QString);
    void turnOn(bool i_on);
    void setAmp(float &amp);
    void setFrequency(float f);
    void startFm(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep);
    void stopFm();
    void setFmMode(int mode);


public slots:
    void newConnection();
    void socketClosed();

private:
    void readData();

    QTcpServer *server;
    QTcpSocket *socket;
    QTextStream in;
    QString command;

    int port;
};

#endif // SERVER_H
