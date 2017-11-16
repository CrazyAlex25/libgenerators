#include "g6009.h"
#include <QDebug>
#include <QThread>

G6009::G6009(QObject *parent)  :
    Generator(0x0403, //vid
                                          0x6001, //pid
                                          9e3, // lowestFreq
                                          6e9, // highestFreq
                                          0.020, // tFmMin
                                          1000, // tFmMax
                                          8e9,  //fFmBandStop
                                          parent),
    referenceFrequency(UnknownRefFreq),
    attenuationMax(63.5),
    attenuationMin(0),
    attenuationStep(0.25)
{

    setSynthLevel(SynthLevelP5);
    calibrator.setBandBorder(25e6);
    attenuator1.id = 0;
    attenuator2.id = 1;

    connectionTimerId = startTimer(500);

}

G6009::~G6009()
{
    if (serialPortInfo != nullptr)
        delete serialPortInfo;

    if (connected)
        turnOn(false);

}


/* Метод проверяет ответ от генератора. Возращает true, если получен
 * ожидаемый ответ.
 */
bool G6009::checkResponse()
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


bool G6009::turnOn(bool i_on)
{
    if (!connected) {
        printMessage("Can't execute command. Generator is not connected.");
        return false;
    }


    double fMHz =  currentFrequency / 1e6;


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

bool G6009::setAmp(double &m_amp)
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

         double attenuation = 20 * log10(maxAmp / m_amp);

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

bool G6009::setAttenuation(double &attenuation)
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

double G6009::getAttenuation()
{
    return NAN;
}

bool G6009::commute(quint8 key)
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

bool G6009:: setFrequency(double &m_fHz)
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
    m_fHz = roundToGrid(m_fHz);
    currentFrequency = m_fHz;
    double fMHz = m_fHz / 1e6;
    double fSynthMHz;

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

    int k = log2(6000 / fSynthMHz);
    int n = 0x8F + (k<<4);
    syntheziser.data[5] = n;

    double tmp1 = std::pow(2,  k) * fSynthMHz;
    int tmp2 = tmp1 /  25 ;
    int tmp3 = std::round(std::fmod(tmp1,  25) * 100);
    quint32 f_Code= ( tmp2 << 15 ) + (  tmp3<< 3 );

    for (qint8 i = sizeof(syntheziser.freq) - 1; i >= 0; --i  )
    {
        syntheziser.freq[ i ] = (quint8)f_Code;
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

    double amp = currentAmp;
    setAmp(amp);
    emit newAmplitude(amp);

       return true;
}


void G6009::setFrequencyGrid(int i_frequencyGrid)
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


FrequencyGrid G6009::getFrequencyGrid()
{
    return frequencyGrid;
}

bool G6009::isG6009(QSerialPortInfo &info)
{

    printMessage("Определяем относится ли данных последовательный порт к РГШ6009...");
    bool success = Generator::connect(info);
    if (!success)
        return success;

    success = commute(LowFrequency);
    success = success && (info.vendorIdentifier() == vid) && (info.productIdentifier() == pid);

    if  (success)
            printMessage("Данный последовательный порт относиться к РГШ6009");
    else
            printMessage("Данный последовательный порт не относиться к РГШ6009");

    serialPort.close();
    delete serialPortInfo;
    serialPortInfo = NULL;
     return success;

}

bool G6009::connect(QSerialPortInfo &info)
{
    bool success = Generator::connect(info);
    if (!success)
        return success;

    connected = commute(LowFrequency);

    if (connected) {
        printMessage( "Генератор успешно подключен");
        calibrator.load(QString(":/G6009/calibration.txt"));
        printMessage("Калибровочная характеристика загружена");
    }

     return connected;
}

void G6009::writeHead()
{
    Head6009 head;
    serialPort.write((char *)&head , sizeof( head));
}

void G6009::setLevelControlMode(LevelControlMode mode)
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

LevelControlMode G6009::getLevelControlMode()
{
    return levelControlMode;
}

void G6009::setSynthLevel(int level)
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
