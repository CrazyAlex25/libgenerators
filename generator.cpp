#include "generator.h"
#include <QDir>
#include <QDebug>

Generator::Generator(int i_vid, int i_pid, double i_lowestFreq, double i_highestFreq, double i_tFmMin, double i_tFmMax, double i_fFmBandStop, QObject *parent) : QObject(parent),
    calibrator(this),
    server(50600, this),
    vid(i_vid),
    pid(i_pid),
    on(false),
    connected(false),
    verbose(false),
    logs(true),
    lowestFrequency(i_lowestFreq),
    highestFrequency(i_highestFreq),
    frequencyGrid(Grid10),
    currentFrequency(NAN),
    currentAmp(0),
    fFmStart(NAN),
    fFmStop(NAN),
    fFmStep(NAN),
    fFm(NAN),
    fFmStopBand (i_fFmBandStop),
    fmMode(UpChirp),
    fmCounter(0),
    tFmMin(i_tFmMin),
    tFmMax(i_tFmMax),
    levelControlMode(Amplitude),
    FmTimerId(-1),
    serialPortInfo(NULL),
    logFileName(QDir::currentPath()+"/log.txt")
{

    QObject::connect(&calibrator, &Calibrator::error, this, &Generator::errorSlot);

    Q_INIT_RESOURCE(amp);
    if(objectCounter == 0) {
        QFile logFile(logFileName);
        if (logFile.open(QFile::WriteOnly)) {
            QTextStream logStream(&logFile);
            logStream << QDateTime::currentDateTime().toString() << "\n";
        }
        logFile.close();
    }

    ++objectCounter;

    QObject::connect(&server, &Server::connected, this, &Generator::serverConnected);
    QObject::connect(&server, &Server::disconnected, this, &Generator::serverDisconnected);
    QObject::connect(&server, &Server::error, this, &Generator::errorSlot );
    QObject::connect(&server, &Server::error, this, &Generator::printMessage );
    QObject::connect(&server, &Server::turnOn, this, &Generator::stateChanged);
    QObject::connect(&server, &Server::setAmp, this, &Generator::amplitudeChanged);
    QObject::connect(&server, &Server::setFrequency, this, &Generator::frequencyChanged);
    QObject::connect(&server, &Server::startFm, this, &Generator::startFm);
    QObject::connect(&server, &Server::stopFm, this, &Generator::stopFm );
    QObject::connect(&server, &Server::setFmMode, this, &Generator::setFmMode );



}

int Generator::objectCounter = 0;

void Generator::printMessage(QString message)
{
    if (verbose)
        qDebug() << message;

    if (logs) {
        QFile logFile(logFileName);
        if (logFile.open(QFile::Append | QFile::Text)) {
            QTextStream logStream(&logFile);
            logStream  << "\n" << message  ;
        }
        logFile.close();
    }

}

bool Generator::connect(QSerialPortInfo &info)
{
    //Обновляем информации о порте
    serialPortInfo = new QSerialPortInfo(info);
    serialPort.setPort(*serialPortInfo);

    bool ok = serialPort.open(QIODevice::ReadWrite);
    if (!ok) {
        QString message = "Не удалось получить доступ к последовательному порту. " + serialPort.errorString();
        printMessage(message);
        emit error(message);
        serialPort.close();
        serialPortInfo = NULL;
        return false;
    }
    printMessage("Последовательный порт открыт.");

    printMessage("Настройка последовательного порта...");

    ok = serialPort.setBaudRate(QSerialPort::Baud115200);
    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить скорость передачи данных"+ serialPort.errorString());
        return false;
    }

    ok = serialPort.setDataBits(QSerialPort::Data8);

    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить информационный разряд"+ serialPort.errorString());
        return false;
    }


    ok = serialPort.setParity(QSerialPort::NoParity);

    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить паритет"+ serialPort.errorString());
        return false;
    }

    printMessage("Последовательный порт открыт.");
    return true;
}

void Generator::timerEvent(QTimerEvent * event)
{

    /* проверяем связь с генератором */
    if (event->timerId() == connectionTimerId) {
        if (serialPortInfo == NULL)
            return;

        QList<QSerialPortInfo> info = QSerialPortInfo::availablePorts();

        bool isInList = false;
        for (int i = 0; i < info.length(); ++i)
            if (info[i].portName()== serialPortInfo->portName())
                isInList = true;

        if (info.isEmpty() || !isInList ) {
            printMessage( "Lost connection" );

            delete serialPortInfo;
            serialPortInfo = NULL;
            serialPort.close();
            qDebug() << serialPort.portName();
            delete serialPortInfo;
            emit disconnected();
        }

    }

    // Переключение качания частоты
    if  (event->timerId() == FmTimerId) {
        fmIteration();
    }

}



void Generator::enableVerbose(bool input)
{
    verbose = input;
}

void Generator::enableLogs(bool input)
{
    logs = input;
}

double Generator::roundToGrid(double f)
{

    double result = 0;

    switch (frequencyGrid)
    {
    case Grid1:
    {
        result = 1e3 * round( f / 1e3);
        break;
    }
    case Grid2:
    {
        result = 2e3 * round( f / 2e3);
        break;
    }
    case Grid5:
    {
        result = 5e3 * round( f / 5e3);
        break;
    }
    case Grid10:
    {
        result = 10e3 * round( f / 10e3);
        break;
    }
    default:
        emit error("Выбранна недопустимая сетка частот");
    }

    return result;
}

QList<QSerialPortInfo> Generator::getAvailablePorts()
{
    return QSerialPortInfo::availablePorts();
}

QSerialPortInfo Generator::getPortInfo()
{
    return *serialPortInfo;
}

void Generator::setFmMode(FmMode mode)
{
    switch (mode) {
    case UpChirp:
        fmMode = mode;
        break;
    case DownChirp:
        fmMode = mode;
        break;
    case FHSS:
        fmMode = mode;
        break;
    default:
        emit error("Выбран неправильный вид ЧМ");
        break;
    }
}

FmMode Generator::getFmMode()
{
    return fmMode;
}

bool Generator::startFm(double &m_fStart, double &m_fStop, double &m_fStep, double &m_timeStep)
{
    if (!connected) {
        printMessage("Can't execute command. Generator is not connected");
        return false;
    }

    if (std::isnan(m_fStart)) {
        emit error("Не установлена нижняя граница");
        return false;
    }

    if (std::isnan(m_fStop)) {
        emit error("Не установлена верхняя граница");
        return false;
    }

    if (std::isnan(m_fStep)) {
        emit error("Не установлен шаг сканирования");
        return false;
    }

    if (std::isnan(m_timeStep)) {
        emit error("Не установлен период сканирования");
        return false;
    }

    if (m_timeStep < tFmMin)
        m_timeStep = tFmMin;

    if (m_timeStep > tFmMax)
        m_timeStep = tFmMax;

    int t_msec = round(m_timeStep * 1e3);
    m_timeStep = t_msec / 1e3;

    fFmStep = m_fStep;
    fFmStart = m_fStart;
    fFmStop = m_fStop;

    if (m_fStart > m_fStop) {
        double tmp = m_fStart;
        m_fStart = m_fStop;
        m_fStop = tmp;
    }


    if ((m_fStop > fFmStopBand) && (m_fStart <= fFmStopBand)) {
        emit error("Сканирование возможно в интервалах [1 МГц; 275МГц) и [275 МГц; 3ГГЦ] ");
        return false;
    }

    if (m_fStart < lowestFrequency)
        m_fStart = lowestFrequency;

    if (m_fStart > highestFrequency)
        m_fStart = highestFrequency;

    if (m_fStop < lowestFrequency)
        m_fStop = lowestFrequency;

    if (m_fStop > highestFrequency)
        m_fStop = highestFrequency;

    m_fStep = roundToGrid(fabs(m_fStep));

    fFmStart = m_fStart;
    fFmStop = m_fStop;
    fFm = fFmStart;
    fmCounter = 0;

//    //  Поиск минимальной амплитуды в полосе
//    double ampMin = ampMax;
//    for (quint64 f = fFmStart; f < fFmStop; f += fFmStep)
//    {
//        int ind = f / fAmpCorrectionStep;

//        if (ampMin > ampCorrection[ind])
//            ampMin = ampCorrection[ind];
//    }

//    // ограничиваем амлитуду
//    if (currentAmp > ampMin)
//        currentAmp = ampMin;


    FmTimerId = startTimer(t_msec);
    tFmStart = QTime::currentTime();
    setFrequency(fFm);
    emit newFrequency(fFm);
    return true;
}


void Generator::fmIteration()
{
    switch (fmMode) {
    case UpChirp:
        fFm = fFmStart + fmCounter * fFmStep;
        ++fmCounter;

        if ((fFm >= fFmStop))
               fmCounter = 0;
        break;

    case DownChirp:
        fFm = fFmStart - fmCounter * fFmStep;
        ++fmCounter;

        if ((fFm <= fFmStart))
            fmCounter = 0;
        break;
    default:
        emit error ("Выбран неправильный вид ЧМ");
        break;
    }


    QTime tFmStop = QTime::currentTime();
    double tFm =  tFmStart.msecsTo(tFmStop) * 1e-3;
    tFmStart = tFmStop;
    emit newTFm(tFm);
    setFrequency(fFm);

    printMessage( "Setted Frequency" + QString::number(fFm) + "Hz");

    emit newFrequency(fFm);
}

void Generator :: stopFm()
{
    if (FmTimerId != -1) {
        killTimer(FmTimerId);
        FmTimerId = -1;
    }

    FmTimerId = -1;
    fFmStart = NAN;
    fFmStop = NAN;
    fFmStep = NAN;
    fFm = NAN;

}


double Generator::getAmp()
{
    return currentAmp;
}

double Generator::getFrequency()
{
    return currentFrequency;
}

double Generator::log2(double x)
{
    return log10(x) / log10(2);
}

void Generator::errorSlot(QString err)
{
    emit error(err);
}

void Generator::disconnect()
{
    serialPort.close();
}

int Generator::getPid()
{
    return pid;
}

int Generator::getVid()
{
    return vid;
}

void  Generator::setTcpPort(int port)
{
    printMessage("Изменен tcp порт: " + QString::number(port));
    server.setPort(port);
    server.start();
}

int Generator::getTcpPort() const
{
    return server.getPort();
}

QHostAddress Generator::getIpAddress() const
{
    return server.getIp();
}

void Generator::amplitudeChanged(double amp)
{
    setAmp(amp);
    emit newAmplitude(amp);
}

void Generator::frequencyChanged(double freq)
{
    setFrequency(freq);
    emit newFrequency(freq);
}

void Generator::stateChanged(bool on)
{
    turnOn(on);
    emit newState(on);
}

void Generator::serverConnected()
{
    emit netControl(on);
}

void Generator::serverDisconnected()
{
    emit netControl(false);
}

void Generator::startServer()
{
    bool success = server.start();
    if (success)
        printMessage("Tcp сервер запущен");
}


void Generator::stopServer()
{
    bool success = server.start();
    if (success)
        printMessage("Tcp сервер, слущающий команды, запущен");
}
