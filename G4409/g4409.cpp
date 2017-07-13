#include "g4409.h"
#include <QDebug>
#include <QThread>

G4409::G4409(QObject *parent)  :
    Generator(0x0403, //vid
                                          0x6001, //pid
                                          9e3, // lowestFreq
                                          6e9, // highestFreq
                                          0.020, // tFmMin
                                          1000, // tFmMax
                                          8e9,  //fFmBandStop
                                          parent),
    synthLevel(SynthLevelP5),
    referenceFrequency(UnknownRefFreq),
    attenuationMax(63.5),
    attenuationMin(0),
    attenuationStep(0.25)
{

    calibrator.setBandBorder(25e6);
    attenuator1.id = 0;
    attenuator2.id = 1;

    connectionTimerId = startTimer(500);

}

G4409::~G4409()
{
    if (serialPortInfo != nullptr)
        delete serialPortInfo;

    if (connected)
        turnOn(false);

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
    if (!connected) {
        printMessage("Can't execute command. Generator is not connected.");
        return false;
    }


    float fMHz =  currentFrequency / 1e6;


    if ( fMHz <= 25){

        if (i_on) {
            syntheziser.data[6] = 0x45;
            syntheziser.data[7] = 0xDC;
        } else {
            syntheziser.data[6] = 0x44;
            syntheziser.data[7] = 0xDC;
        }


    } else {

        if (i_on) {
            syntheziser.data[6] = 0x44;
            syntheziser.data[7] = synthLevel;
        } else {
            syntheziser.data[6] = 0x44;
            syntheziser.data[7] = 0xDC;
        }

    }


    serialPort.write((char *)&syntheziser, sizeof(syntheziser));
    bool status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "First syntheziser buffer: ";
        for (uint i = 0; i < sizeof(syntheziser.data); ++i)
            qDebug() << QString("%1").arg(syntheziser.data[i], 2, 16, QChar('0'));
        for (uint i = 0; i < sizeof(syntheziser.freq); ++i)
           qDebug() << QString("%1").arg(syntheziser.freq[i], 2, 16, QChar('0'));
    #endif

    if (!status)
           return false;

    if (i_on)
        printMessage("Generator turned on");
    else
        printMessage("Generator turned off");


    on = i_on;
    return true;
}

bool G4409::setAmp(float &m_amp)
{
    currentAmp = m_amp;
    bool success = false;

    /* В зависимости от режима управления уровнем сигнала либо идет пересчет
     * амплитуды в ослабление аттенюатора, либо нет.
     */
    switch (levelControlMode)
    {
    case Amplitude:
    {
         double maxAmp =calibrator.getAmp(currentFrequency);

         if (m_amp > maxAmp)
             m_amp = maxAmp;

         float attenuation = 20 * log10(maxAmp / m_amp);

         success = setAttenuation(attenuation);

         m_amp = maxAmp / pow(10, attenuation / 20);


        break;
    }
    case Attenuation:
    {
        success = setAttenuation(m_amp);
    }
    }
    return success;
}

bool G4409::setAttenuation(float &attenuation)
{
    if (!connected) {
        printMessage("Can't set attenuator. Generator is not connected.");
        return false;
    }

    if (attenuation > attenuationMax )
        attenuation = attenuationMax;

    if (attenuation < attenuationMin)
        attenuation = attenuationMin;

    attenuation = round(attenuation / attenuationStep) * attenuationStep; // округление до шага аттенюатора

    quint8 halfRange = attenuationMax * 2;
    attenuator1.data = (quint8) (attenuation * 4);
    if (attenuator1.data > halfRange)
        attenuator1.data = halfRange;
    attenuator2.data = ((quint8) (attenuation * 4)) - attenuator1.data;

    serialPort.write((char *)&attenuator1, sizeof(attenuator1) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "Attenuation buffer: ";
       for (uint i = 0; i < sizeof(attenuator1.data); ++i)
        qDebug() << QString("%1").arg(attenuator1.data, 2, 10, QChar('0'));
    #endif


    bool  success = checkResponse();
    if (!success) {
        //emit error ("Не удалось установить амплитуду");
        return false;
    }

    serialPort.write((char *)&attenuator2, sizeof(attenuator2) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "atttenuation buffer: ";
       for (uint i = 0; i < sizeof(attenuator2.data); ++i)
        qDebug() <<QString("%1").arg(attenuator2.data, 2, 10, QChar('0'));
    #endif

    success = checkResponse();
    if (!success) {
        //emit error ("Не удалось установить амплитуду");
        return false;
    }

   printMessage( "Setted Attenuation  " + QString::number(attenuation) + "dB.");
    return true;
}

float G4409::getAttenuation()
{
    return NAN;
}

bool G4409::commute(quint8 key)
{
    if (on) {
        switch (key)
        {
        case LowFrequency:
            {
            switcher.value = 1;
            break;
            }
        case HighFrequency:
            {
            switcher.value = 0;
            break;
            }
        default:

          printMessage("Wrong key for switcher");

        }
    } else {
        switcher.value = 2;
    }


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

bool G4409:: setFrequency(float &m_fHz)
{
    if (!connected) {
        printMessage("Can't execute command. Generator is not connected.");
        return false;
    }

    // Обработка входных данных
    if ( m_fHz < lowestFrequency)
        m_fHz = lowestFrequency;
    if ( m_fHz > highestFrequency )
        m_fHz = highestFrequency;

    // Округление до деления выбранной сетки
//    m_freq = roundToGrid(m_freq);

    currentFrequency = m_fHz;
    float fMHz = m_fHz / 1e6;
    float fSynthMHz;

    // Формирование команд на нужный режим работ генератора
    if ( fMHz <= 25){

        // НАстройка синтезатора
      fSynthMHz = 100;

      if (on) {
          syntheziser.data[6] = 0x45;
          syntheziser.data[7] = 0xDC;
      }
      // Настройка DDS
      int fDds = std::pow(2, 32) * (fMHz / 100);
      for (qint8 k = sizeof(dds.freq) - 1; k >= 0; --k  )
      {
      dds.freq[ k ] = (quint8)fDds;
      fDds = fDds >> 8;
      }

    } else {
        // Настройка синтезатора
        fSynthMHz = fMHz;

        if (on) {
            syntheziser.data[6] = 0x44;
            syntheziser.data[7] = synthLevel;
        }
    }

        quint16 k = log2(6000 / fSynthMHz);
        quint16 n = 0x8F + (k<<4);
        syntheziser.data[5] = n;

        int tmp1 = std::pow(2, k) * fSynthMHz;
        int tmp2 = tmp1 /  25 ;
        int tmp3 = ( tmp1  %  25) * 100;
        quint32 f_Code= ( tmp2 << 15 ) + (  tmp3<< 3 );

        for (qint8 k = sizeof(syntheziser.freq) - 1; k >= 0; --k  )
        {
        syntheziser.freq[ k ] = (quint8)f_Code;
        f_Code = f_Code >> 8;
        }



    serialPort.write((char *)&syntheziser, sizeof(syntheziser));
    bool status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "First syntheziser buffer: ";
        for (uint i = 0; i < sizeof(syntheziser.data); ++i)
            qDebug() << QString("%1").arg(syntheziser.data[i], 2, 16, QChar('0'));
        for (uint i = 0; i < sizeof(syntheziser.freq); ++i)
           qDebug() << QString("%1").arg(syntheziser.freq[i], 2, 16, QChar('0'));
    #endif

    if (!status)
           return false;

    if (fMHz <= 25) {
        //  Отправка команды DDS
        serialPort.write((char *)&dds, sizeof(dds));
        bool status = checkResponse();

        #ifdef QT_DEBUG
            qDebug() << "DDS buffer: ";
            qDebug() << QString("%1").arg(dds.phase, 2, 16, QChar('0'));
            for (uint i = 0; i < sizeof(dds.freq); ++i)
                qDebug() << QString("%1").arg(dds.freq[i], 2, 16, QChar('0'));
        #endif

       if (!status)
            return false;

       commute(LowFrequency);
    } else {
        commute(HighFrequency);
    }

    printMessage("Setted Frequency " + QString::number(fMHz) + "МHz");

    float amp = currentAmp;
    setAmp(amp);
    emit newAmplitude(amp);

       return true;
}


void G4409::setFrequencyGrid(int i_frequencyGrid)
{
    switch (i_frequencyGrid)
    {
    case Grid10:
    {
        frequencyGrid = Grid10;
        break;
    }
    default:
        emit error("Выбранна недопустимая сетка частот");
    }
}


FrequencyGrid G4409::getFrequencyGrid()
{
    return frequencyGrid;
}

bool G4409::isG4409(QSerialPortInfo &info)
{
    //Обновляем информации о порте
    serialPortInfo = new QSerialPortInfo(info);
    serialPort.setPort(*serialPortInfo);

    bool ok = serialPort.open(QIODevice::ReadWrite);
    if (!ok) {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        QString message = "Не удалось получить доступ к последовательному порту. " + serialPort.errorString();
        emit error(message);
        printMessage(message);
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

    bool success = commute(LowFrequency);

    serialPort.close();
    delete serialPortInfo;
    serialPortInfo = NULL;
    return  success;


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


    connected = commute(LowFrequency);

    if (connected) {
        printMessage( "Generator has been connected");
        calibrator.load(QString(":/G4409/calibration.txt"));
        printMessage("Amp calibration loaded");
    }

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
    switch (mode) {
    case Amplitude:
        levelControlMode = Amplitude;
        break;
    case Attenuation:
        levelControlMode = Attenuation;
        break;
    default:
        emit error("Задан неправильный режим управление уровнем сигнала");
        break;
    }
}

LevelControlMode G4409::getLevelControlMode()
{
    return levelControlMode;
}

void G4409::setSynthLevel(int level)
{
    switch (level)
    {
    case Off:
        synthLevel = 0xDC;
        break;
    case SynthLevelM4:
        synthLevel = 0xE4;
        break;
    case SynthLevelM1:
        synthLevel = 0xEC;
        break;
    case SynthLevelP2:
        synthLevel = 0xF4;
        break;
    case SynthLevelP5:
        synthLevel = 0xFC;
        break;
    }
}
