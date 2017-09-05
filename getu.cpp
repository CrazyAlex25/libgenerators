#include "getu.h"

GetU::GetU(QObject *parent) : G3000(0x10c4, 0xea60, parent),
    timeOutTimerId(-1),
    poolPeriod(4000),
    freq(NAN)
{

}


void GetU::timerEvent(QTimerEvent *event)
{
    if(timeOutTimerId == event->timerId()) {
        killTimer(timeOutTimerId);
        G3000::setFrequency(freq);
    }
}

bool GetU::turnOn(bool i_on)
{
    if (timeOutTimerId != -1)
        killTimer(timeOutTimerId);

    if (i_on)
        timeOutTimerId = startTimer(poolPeriod);

    return G3000::turnOn(i_on);
}

bool GetU::setAmp(float &m_amp)
{
    if (timeOutTimerId != -1)
        killTimer(timeOutTimerId);

    timeOutTimerId = startTimer(poolPeriod);
    return G3000::setAmp(m_amp);
}

bool GetU::setFrequency(float &m_f)
{
    if (timeOutTimerId != -1)
        killTimer(timeOutTimerId);

    timeOutTimerId = startTimer(poolPeriod);

    freq = m_f;
    return G3000::setFrequency(m_f);
}


bool GetU::isGetU(QSerialPortInfo &info)
{
    printMessage("Определяем относится ли данных последовательный порт к Гетеродину У.");
    bool success = Generator::connect(info);
    if (!success)
        return success;

    success = commute(1);
    success = success && (info.vendorIdentifier() == vid) && (info.productIdentifier() == pid);

    if  (success)
            printMessage("Данный последовательный порт относиться к Гетеродину У");
    else
            printMessage("Данный последовательный порт не относиться к Гетеродину У");


    serialPort.close();
    delete serialPortInfo;
    serialPortInfo = NULL;
     return success;
}
