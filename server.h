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
    void setPort(int i_port);
    int getPort() const;
    QHostAddress getIp() const;
    bool start();

signals:
    void connected();
    void disconnected();
    void error(QString);
    void turnOn(bool i_on);
    void setAmp(double &amp);
    void setFrequency(double f);
    void startFm(double &m_fStart, double &m_fStop, double &m_fStep, double &m_timeStep);
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
