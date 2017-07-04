#include "generator.h"
#include <QDir>
#include <QTextStream>
#include <QDebug>

Generator::Generator(int i_vid, int i_pid, float i_lowestFreq, float i_highestFreq, float i_tFmMin, float i_tFmMax, float i_fFmBandStop, QObject *parent) : QObject(parent),
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
    fAmpCorrectionStep(NAN),
    FmTimerId(-1),
    serialPortInfo(NULL),
    logFileName(QDir::currentPath()+"/log.txt")
{

    Q_INIT_RESOURCE(amp);
    QFile logFile(logFileName);
    if (logFile.open(QFile::WriteOnly)) {
        QTextStream logStream(&logFile);
        logStream << QDateTime::currentDateTime().toString() << "\n";
    }
    logFile.close();

}

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
            emit disconnected();
        }

    }

    // Переключение качания частоты
    if  (event->timerId() == FmTimerId) {

        switch (fmMode) {
        case UpChirp:
            fFm = fFmStep;

            if ((fFm >= fFmStop))
                fFm = fFmStart + fmCounter * fFmStep;
            break;

        case DownChirp:
            fFm = fFmStart - fmCounter * fFmStep;

            if ((fFm <= fFmStart))
                fFm = fFmStop;
            break;
        default:
            emit error ("Выбран неправильный вид ЧМ");
            break;
        }

        QTime tFmStop = QTime::currentTime();
        float tFm =  tFmStart.msecsTo(tFmStop) * 1e-3;
        tFmStart = tFmStop;
        emit newTFm(tFm);
        setFrequency(fFm);

        printMessage( "Setted Frequency" + QString::number(fFm) + "Hz");

        emit newFrequency(fFm);
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

float Generator::roundToGrid(float f)
{

    float result = 0;

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

bool Generator::startFm(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep)
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
        float tmp = m_fStart;
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
//    float ampMin = ampMax;
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

void Generator :: stopFm()
{
    if (FmTimerId != -1)
        killTimer(FmTimerId);

    FmTimerId = -1;
    fFmStart = NAN;
    fFmStop = NAN;
    fFmStep = NAN;
    fFm = NAN;

}

void Generator:: loadCalibrationAmp(QString fileName)
{

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        emit error("Не найден файл грубой калибровки");

    QTextStream in(&file);
    float f1;
    float f2;
    double P_dBm;
    double amp_V;
    in >> f1;
    in >> P_dBm;
    amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
    ampCorrection.push_back(amp_V);

    if (amp_V > ampMax)
        ampMax = amp_V;

    in >> f2;
    in >> P_dBm;
    amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
    ampCorrection.push_back(amp_V);
    fAmpCorrectionStep = (f2 - f1) * 1e6;


    if (amp_V > ampMax)
        ampMax = amp_V;

    while (!in.atEnd())
    {
        in >> f1;
        in >> P_dBm;
        amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
        ampCorrection.push_back(amp_V);


        if (amp_V > ampMax)
            ampMax = amp_V;
    }
}


double Generator::getAmpCorrection()
{
    if (std::isnan(currentFrequency)) {
        return ampCorrection[1];
    } else {
        int ind = round(currentFrequency / fAmpCorrectionStep);
        return ampCorrection[ind];
    }

}


float Generator::getAmp()
{
    return currentAmp;
}

float Generator::getFrequency()
{
    return currentFrequency;
}

double Generator::log2(double x)
{
    return log10(x) / log10(2);
}
