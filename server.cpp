#include "server.h"

Server::Server(int i_port, QObject *parent) : QObject(parent),
    server(nullptr),
    socket(nullptr),
    port(i_port)
{

}

bool Server::start()
{
    if (server != nullptr)
        delete server;

    server = new QTcpServer(this);
    bool success =  server->listen(QHostAddress::Any, port);

    if (!success) {
        emit error( " Не удалось запустить сервер. Ошибка: " + server->errorString());
        return false;
    }

    connect(server, &QTcpServer::newConnection, this, &Server::newConnection);
    return true;

}

void Server::stop()
{
    server->close();
    delete server;
    disconnect(server, &QTcpServer::newConnection, this, &Server::newConnection);
}

void  Server::setPort(int i_port)
{
    port = i_port;
}

int Server::getPort() const
{
    return port;
}

QHostAddress Server::getIp() const
{
    return server->serverAddress();
}

void  Server::readData()
{
    in >> command;

    while (!command.isEmpty())
    {
        switch (command[0].toLatin1()) {
        case 'F':
        {
            QString freqStr;
            for (int i = 1; i < command.length(); ++i)
            {
               freqStr.append(command[i]);
            }

            bool success;
            double  freqHz = freqStr.toDouble(&success);
            if (! success) {
                emit error("Не удалось распознать команду: " + command);
                return;
            }

            emit setFrequency(freqHz);
            break;
        }
        case 'A':
        {
            QString ampStr;
            for (int i = 1; i < command.length(); ++i)
            {
               ampStr.append(command[i]);
            }

            bool success;
            double  ampV = ampStr.toDouble(&success);
            if (! success) {
                emit error("Не удалось распознать команду: " + command);
                return;
            }

            emit setAmp(ampV);
            break;
         }
        case 'S':
        {
            switch (command[1].toLatin1())
            {
            case '1':
                emit turnOn(true);
                break;
            case '0':
                emit turnOn(false);
                break;
            default:
                emit error("Не удалось распознать команду: " + command);
            }
        }
            break;
        case 'M':
        {
            QString pattern = "ModF";
            QString header;
            for (int i = 0; i < pattern.length(); ++i)
                header.append(command[i]);

            if (header == pattern) {

                // "ModF0" выключение качания частоты
                if(command[4] == '0') {
                    emit stopFm();
                    return;
                }

                int i = 4;
                QString fStartStr;
                while (command[i] != ',') {

                   if (i == command.length()) {
                       emit error("Не удалось распознать команду: " + command);
                       return;
                   }

                   fStartStr.append(command[i]);
                   ++i;
                }

                ++i;
                QString fStopStr;
                while (command[i] != ',') {

                   if (i == command.length()) {
                       emit error("Не удалось распознать команду: " + command);
                       return;
                   }

                   fStopStr.append(command[i]);
                   ++i;
                }

                ++i;
                QString fStepStr;
                while (command[i] != ',') {

                   if (i == command.length()) {
                       emit error("Не удалось распознать команду: " + command);
                       return;
                   }

                   fStepStr.append(command[i]);
                   ++i;
                }

                ++i;
                QString tStr;
                while (i != command.length()) {

                   if (i == (command.length())) {
                       emit error("Не удалось распознать команду: " + command);
                       return;
                   }

                   tStr.append(command[i]);
                   ++i;
                }

                bool success;
                double  fStart = fStartStr.toDouble(&success);
                if (! success) {
                    emit error("Не удалось распознать команду: " + command);
                    return;
                }

                double  fStop = fStopStr.toDouble(&success);
                if (! success) {
                    emit error("Не удалось распознать команду: " + command);
                    return;
                }

                double  fStep = fStepStr.toDouble(&success);
                if (! success) {
                    emit error("Не удалось распознать команду: " + command);
                    return;
                }

                double  t = tStr.toDouble(&success);
                if (! success) {
                    emit error("Не удалось распознать команду: " + command);
                    return;
                }
                emit startFm(fStart, fStop, fStep, t);

            }
            break;
        }
        default:
                emit error("Не удалось распознать команду: " + command);
        }

        in >> command;
    }


}

void Server::newConnection()
{
    socket = server->nextPendingConnection();
    connect(socket, &QAbstractSocket::disconnected, socket, &QObject::deleteLater);
    connect(socket, &QAbstractSocket::disconnected, this, &Server::socketClosed);
    connect(socket, &QAbstractSocket::readyRead, this , &Server::readData);

    in.setDevice(socket);

    emit connected();

}

void Server::socketClosed()
{
    emit disconnected();
}
