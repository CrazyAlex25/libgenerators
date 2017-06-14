#include "g4409.h"
#include <cmath>
#include <QDebug>
#include <QThread>

G4409::G4409(QObject *parent)  :
    Generator(0x0403, //vid
                                          0x6001, //pid
                                          9e3, // lowestFreq
                                          4.4e9, // highestFreq
                                          0.020, // tSweepMin
                                          1000, // tSweepMax
                                          parent),
    referenceFrequency(UnknownRefFreq),
    attenuationMax(63),
    attenuationMin(0),
    attenuationStep(0.5)
{

    Q_INIT_RESOURCE(amp);


    syntheziser1.id= 0x01;

    syntheziser2.id = 0x02;

    connectionTimerId = startTimer(500);
}

G4409::~G4409()
{
    if (connected) {
        turnOn(false);
        delete serialPortInfo;
    }
}


/* Метод проверяет ответ от генератора. Возращает true, если получен
 * ожидаемый ответ.
 */
bool G4409::checkResponse()
{
    // Ждем ответа от генератора
    bool  success = true;
    success &= serialPort.waitForReadyRead(3000);

    if (!success) {
        printMessage("No response from generator");
        return false;
    }

    // Считываем ответ
    char responseFromGen[sizeof(response) + 1] = {0};
    this->thread()->usleep(20);

    qint64 bytesToRead = serialPort.read(responseFromGen, sizeof(responseFromGen));


    if (bytesToRead == -1) {
        printMessage("Can't read data from searial port");
        success &= false;
    }

    //Если ответ принят не полностью, делаем второй запрос данных
    if (bytesToRead != sizeof(response)) {
        this->thread()->usleep(30);
        char responseFromGen2[sizeof(response) + 1] = {0};
        serialPort.waitForReadyRead(30);
        qint64 bytesToRead2 = serialPort.read(responseFromGen2, sizeof(responseFromGen2));

        // Склеиваем
        if (bytesToRead2 != 0) {
            for (uint i = bytesToRead; i < sizeof(responseFromGen); ++i)
                responseFromGen[i] = responseFromGen2[i - bytesToRead];
        }
    }

    // Сравнения с эталонным ответом
    for (uint i = 0; i < sizeof(response); ++i)
    {
        unsigned char symbol = responseFromGen[i];
        if (response.data[i] !=  symbol) {
            success = false;
            break;
        }
    }


    // Очищаем буффер обмена.
    serialPort.readAll();

    return success;
}


bool G4409::turnOn(bool i_on)
{
    return false;
}

bool G4409::setAmp(float &m_amp)
{
    Test test;
    serialPort.write((char *)&test, sizeof(test));

#ifdef QT_DEBUG
    qDebug() << "First syntheziser buffer: ";
    for (uint i = 0; i < sizeof(test.data); ++i)
        qDebug() << QString("%1").arg(test.data[i], 2, 16, QChar('0'));
     for (uint i = 0; i < sizeof(test.freq); ++i)
        qDebug() << QString("%1").arg(test.freq[i], 2, 16, QChar('0'));
#endif

    return checkResponse();
}

float G4409::getAmp()
{
    return currentAmp;
}

bool G4409::setAttenuation(float &attenuation)
{
    return false;
}

float G4409::getAttenuation()
{
    return NAN;
}

bool G4409::commute(quint8 key)
{
    //if (on)
    if (true)

        switch (key)
        {
        case LowBand:
            {
            switcher.value = 1;
            break;
            }
        case HighBand:
            {
            switcher.value = 0;
            break;
            }
        default:

          printMessage("Wrong key for switcher");

        }
    else
        switcher.value = 2;



    // Передаем его в генератор
    serialPort.write((char *)&switcher , sizeof( switcher));

#ifdef QT_DEBUG
    qDebug() << "Switcher buffer: ";
        qDebug() << QString("%1").arg(switcher.value, 2, 16, QChar('0'));
#endif


    // Проверяем ответ
    bool success;
    success = checkResponse();
    if (!success) {
        emit error ("Возникла ошибка при включении");
        return false;
    }

    return true;
}

bool G4409::setFrequency(float &m_fHz)
{
//    if (!connected) {
//        printMessage("Can't execute command. Generator is not connected.");
//        return false;
//    }

    // Обработка входных данных
    if ( m_fHz < lowestFrequency)
        m_fHz = lowestFrequency;
    if ( m_fHz > highestFrequency )
        m_fHz = highestFrequency;

    // Округление до деления выбранной сетки
//    m_freq = roundToGrid(m_freq);

    currentFrequency = m_fHz;
    int fMHz = m_fHz / 1e6;

    quint16 k = log2(6000 / fMHz);
    quint16 n = 0x8F + (k<<4);
    syntheziser1.data[5] = n;
    syntheziser1.data[6] = 0x45;
    syntheziser1.data[7] = 0xFC;
    int tmp1 = std::pow(2, k);
    int tmp2 = (tmp1 * fMHz ) /  25 ;
    int tmp3 = ((tmp1 * fMHz)  %  25) * 100;
    quint32 f_Code= ( tmp2 << 15 ) + (  tmp3<< 3 );

    for (qint8 k = sizeof(syntheziser1.freq) - 1; k >= 0; --k  )
    {
    syntheziser1.freq[ k ] = (quint8)f_Code;
    f_Code = f_Code >> 8;
    }

    serialPort.write((char *)&syntheziser1, sizeof(syntheziser1));
    bool status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "First syntheziser buffer: ";
        for (uint i = 0; i < sizeof(syntheziser1.data); ++i)
            qDebug() << QString("%1").arg(syntheziser1.data[i], 2, 16, QChar('0'));
        for (uint i = 0; i < sizeof(syntheziser1.freq); ++i)
           qDebug() << QString("%1").arg(syntheziser1.freq[i], 2, 16, QChar('0'));
    #endif

    if (!status)
           return false;

    printMessage("Setted Frequency " + QString::number(fMHz) + "МHz");

    float amp = currentAmp;
    //setAmp(amp);
    emit newAmplitude(amp);

       return true;
}

float G4409::getFrequency()
{
    return currentFrequency;
}

bool G4409::startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode)
{
    return false;
}

void G4409::stopFrequencySweep()
{

}

void G4409::setFrequencyGrid(int i_frequencyGrid)
{

}


FrequencyGrid G4409::getFrequencyGrid()
{
    return 0;
}

bool G4409::connect(QSerialPortInfo &info)
{
    //Обновляем информации о порте
    serialPortInfo = new QSerialPortInfo(info);
    serialPort.setPort(*serialPortInfo);

    bool ok = serialPort.open(QIODevice::ReadWrite);
    if (!ok) {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось получить доступ к последовательному порту. " + serialPort.errorString());
        return false;
    }



    ok = serialPort.setBaudRate(QSerialPort::Baud115200);
    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить скорость передачи данных");
        return false;
    }

    ok = serialPort.setDataBits(QSerialPort::Data8);

    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить информационный разряд");
        return false;
    }


    ok = serialPort.setParity(QSerialPort::NoParity);

    if (!ok)
    {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        emit error("Не удалось установить паритет");
        return false;
    }

    serialPort.write((char *)&reset , sizeof( reset));
    connected = checkResponse();

    printMessage( "Generator has been connected");


   // loadCalibrationAmp();

   // printMessage("Amp calibration loaded");


     return connected;
}

void G4409::disconnect()
{
    serialPort.close();
    this->thread()->sleep(1);
}


void G4409::writeHead()
{
    Head4409 head;
    serialPort.write((char *)&head , sizeof( head));
}

void G4409::setLevelControlMode(LevelControlMode mode)
{

}

LevelControlMode G4409::getLevelControlMode()
{
    return 0;
}
