#include "G3000/g3000.h"
#include <QDebug>

G3000::G3000(QObject *parent) :
    Generator(0x0403, //vid
                                          0x6001, //pid
                                          1e6, // lowestFreq
                                          3e9, // highestFreq
                                          0.020, // tFmMin
                                          1000, // tFmMax
                                          275e6, //fFmBandStop
                                          parent)
{
    initialize();
}

G3000::G3000(int i_vid, int i_pid, QObject *parent) :
    Generator(i_vid, //vid
                                          i_pid, //pid
                                          1e6, // lowestFreq
                                          3e9, // highestFreq
                                          0.020, // tFmMin
                                          1000, // tFmMax
                                          275e6, //fFmBandStop
                                          parent)
{
    initialize();
}

G3000::~G3000()
{
    if (serialPortInfo != nullptr)
        delete serialPortInfo;

    if (connected)
        turnOn(false);

}

void G3000::initialize()
{
    referenceFrequency = UnknownRefFreq;
    attenuationMax = 63;
    attenuationMin = 0;
    attenuationStep = 0.5;

    calibrator.setBandBorder(275e6);

    setFrequencyGrid(Grid1);
    syntheziser1.data[7] = 36;
    //commutator = 0;

    syntheziser1.id[0] = 0x31;
    syntheziser1.id[1] = 0x31;

    syntheziser2.id[0] = 0x31;
    syntheziser2.id[1] = 0x32;

    connectionTimerId = startTimer(500);
}

bool G3000::isG3000(QSerialPortInfo &info)
{
    printMessage("Определяем относится ли данных последовательный порт к РГШ3000...");
    bool success = Generator::connect(info);
    if (!success)
          return success;


    success = commute(1);
    success = success && (info.vendorIdentifier() == vid) && (info.productIdentifier() == pid);

    if  (success)
            printMessage("Данный последовательный порт относиться к РГШ3000");
    else
            printMessage("Данный последовательный порт не относиться к РГШ3000");

    serialPort.close();
    delete serialPortInfo;
    serialPortInfo = NULL;

    return success;


}

bool G3000::connect(QSerialPortInfo &info)
{
    bool success = Generator::connect(info);
    if (!success)
          return success;


    connected = commute(1);

    if (connected ) {
        printMessage( "Генератор подключен");
        calibrator.load(":/G3000/calibration.txt");
        printMessage("Калибровачная характеристика загружена");
    }

    return connected;
}

/* Включение/выключение генератора*/
bool G3000::turnOn(bool i_on)
{

    if (!connected) {
        printMessage( "Can't execute command. Generator is not connected");
        return false;
    }

    on = i_on;
    return commute(DirectSignal);

}

/* Коммутация генератора в зависимости от режима работы*/
bool G3000::commute(quint8 key)
{
    if (on)

        switch (key)
        {
        case DirectSignal:
            {
            switcher.value = 1;
            break;
            }
        case DiffSignal:
            {
            switcher.value = 7;
            break;
            }
        default:

          printMessage("Wrong key for switcher");

        }
    else
        switcher.value = 0;



    // Передаем его в генератор
    serialPort.write((char *)&switcher , sizeof( switcher));

    #ifdef QT_DEBUG
       qDebug() << "Switcher buffer: ";
       for (uint i = 0; i < sizeof(switcher.value); ++i)
        qDebug() << switcher.value;
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

// Установка амлитуды. Функция возвращает значение реально установленной амплитуды
bool G3000::setAmp(double &amp)
{
    currentAmp = amp;
    bool success = false;

    /* В зависимости от режима управления уровнем сигнала либо идет пересчет
     * амплитуды в ослабление аттенюатора, либо нет.
     */
    switch (levelControlMode)
    {
    case Amplitude:
    {
         double maxAmp = calibrator.getAmp(currentFrequency);

         if (amp > maxAmp)
             amp = maxAmp;

         double attenuation = 20 * log10(maxAmp / amp);

         success = setAttenuation(attenuation);

         double koef  = pow (10.0, attenuation / 20.0);
         amp = maxAmp / koef ;


        break;
    }
    case Attenuation:
    {
        success = setAttenuation(amp);
    }
    }
    return success;

}

// Установка значений аттенюатора. Функция возвращает значение реально установленной амплитуды
bool G3000::setAttenuation(double &attenuation)
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
    attenuator1.data = (quint8) (attenuation);
    attenuator2.data = ((quint8) (attenuation * 2)) - attenuator1.data;

    attenuator2.data += 128;

    serialPort.write((char *)&attenuator1, sizeof(attenuator1) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "Attenuation 1 buffer: ";
       for (uint i = 0; i < sizeof(attenuator1.data); ++i)
        qDebug() << attenuator1.data;
    #endif


    bool  success = checkResponse();
    if (!success) {
        //emit error ("Не удалось установить амплитуду");
        return false;
    }


    serialPort.write((char *)&attenuator2, sizeof(attenuator2) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "atttenuation 2 buffer: ";
       for (uint i = 0; i < sizeof(attenuator2.data); ++i)
        qDebug() << attenuator2.data;
    #endif

    success = checkResponse();
    if (!success) {
        //emit error ("Не удалось установить амплитуду");
        return false;
    }

   printMessage( "Setted Attenuation  " + QString::number(attenuation) + "dB.");
    return true;
}

/* Метод проверяет ответ от генератора. Возращает true, если получен
 * ожидаемый ответ.
 */
bool G3000::checkResponse()
{
    // Ждем ответа от генератора
    bool  success = true;
    success &= serialPort.waitForReadyRead(100);

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
    if (bytesToRead != sizeof(responseFromGen)) {
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
        if (response.data[i] != responseFromGen[i]) {
            success = false;
            break;
        }
    }


    // Очищаем буффер обмена.
    serialPort.readAll();

    return success;
}

/* Установка частоты. Возвращает значение реально установленной частоты */
bool G3000 :: setFrequency(double &m_freq)
{
    if (!connected) {
        printf("Can't execute command. Generator is not connected.");
        return false;
    }
    syntheziser1.data[15] = 0x42;

    // Обработка входных данных
    if ( m_freq < lowestFrequency)
        m_freq = lowestFrequency;
    if ( m_freq > highestFrequency )
        m_freq = highestFrequency;

    // Округление до деления выбранной сетки
    m_freq = roundToGrid(m_freq);

    currentFrequency = m_freq;
    double f = m_freq;

    // Выставление опорного генератора
    if (f < 275e6) {
        //double referenceFrequency_old = referenceFrequency;
        if ( ((f > 155e6) && (f < 160e6)) ||
             ((f > 180e6) && (f < 186e6)) ||
             ((f > 215e6) && (f < 225e6)) ||
             ( f > 270e6)) {
             referenceFrequency = getReferenceFrequency(RefFreq1150);
             syntheziser2.data[21] = 46;
        } else {
             referenceFrequency = getReferenceFrequency(RefFreq1100);
             syntheziser2.data[21] = 44;
        }
    }

    // Установка синтезаторов

    int moduleV = 0;
    int remainderV = 0;
    int R = 0;

    switch (frequencyGrid)
    {
    case Grid1:
    {
        syntheziser1.data[18] = 225;
        syntheziser1.data[19] = 169;

        moduleV = 3125;

        if ( f < 274999e3 ) {
            f = referenceFrequency - f;
            commute(DiffSignal);
        } else {
            commute(DirectSignal);
        }

        if ( f < 549999e3) {
            syntheziser1.data[13] = 0;
            syntheziser1.data[14] = 94;
            syntheziser1.data[5] = 191;
            remainderV = 8;
            R = 1;

            if ( f < 1099999e3) {
                syntheziser1.data[13] = 0;
                syntheziser1.data[14] = 136;
                syntheziser1.data[5] = 175;
                remainderV = 4;
                R = 2;

                if ( f < 2199999e3) {
                    syntheziser1.data[13] = 1;
                    syntheziser1.data[14] = 16;
                    syntheziser1.data[5] = 159;
                    remainderV = 2;
                    R = 4;
                } else {
                    syntheziser1.data[13] = 2;
                    syntheziser1.data[14] = 30;
                    syntheziser1.data[5] = 143;
                    remainderV = 1;
                    R = 8;
                }
            }
        }

        break;
    }
    case Grid2:
    {
        syntheziser1.data[18] = 225;
        syntheziser1.data[19] = 169;

        moduleV = 3125;

        if ( f < 274998e3 ) {
            f = referenceFrequency - f;
            commute(DiffSignal);
        } else {
            commute(DirectSignal);
        }

        if ( f < 549998e3) {
            syntheziser1.data[13] = 0;
            syntheziser1.data[14] = 94;
            syntheziser1.data[5] = 191;
            remainderV = 8;
            R = 1;

            if ( f < 1099998e3) {
                syntheziser1.data[13] = 0;
                syntheziser1.data[14] = 94;
                syntheziser1.data[5] = 175;
                remainderV = 4;
                R = 1;

                if ( f < 2199998e3) {
                    syntheziser1.data[13] = 0;
                    syntheziser1.data[14] = 136;
                    syntheziser1.data[5] = 159;
                    remainderV = 2;
                    R = 2;
                } else {
                    syntheziser1.data[13] = 1;
                    syntheziser1.data[14] = 16;
                    syntheziser1.data[5] = 143;
                    remainderV = 1;
                    R = 4;
                }
            }
        }
        break;
    }
    case Grid5:
    {
        syntheziser1.data[18] = 206;
        syntheziser1.data[19] = 33;

        moduleV = 2500;

        if ( f < 274995e3 ) {
            f = referenceFrequency - f;
            commute(DiffSignal);
        } else {
            commute(DirectSignal);
        }

        if ( f < 549995e3) {
            syntheziser1.data[13] = 0;
            syntheziser1.data[14] = 94;
            syntheziser1.data[5] = 191;
            remainderV = 8;
            R = 1;

            if ( f < 1099995e3) {
                syntheziser1.data[13] = 0;
                syntheziser1.data[14] = 94;
                syntheziser1.data[5] = 175;
                remainderV = 4;
                R = 1;

                if ( f < 2199995e3) {
                    syntheziser1.data[13] = 0;
                    syntheziser1.data[14] = 94;
                    syntheziser1.data[5] = 159;
                    remainderV = 2;
                    R = 1;
                } else {
                    syntheziser1.data[13] = 0;
                    syntheziser1.data[14] = 136;
                    syntheziser1.data[5] = 143;
                    remainderV = 1;
                    R = 2;
                }
            }
        }

        break;
    }
    case Grid10:
    {
        syntheziser1.data[18] = 206;
        syntheziser1.data[19] = 33;

        moduleV = 2500;

        if ( f < 274990e3 ) {
            f = referenceFrequency - f;
            commute(DiffSignal);
        } else {
            commute(DirectSignal);
        }

        if ( f < 549900e3) {
            syntheziser1.data[13] = 0;
            syntheziser1.data[14] = 94;
            syntheziser1.data[5] = 191;
            remainderV = 8;
            R = 1;
        } else {
            if ( f < 1099990e3) {
                syntheziser1.data[13] = 0;
                syntheziser1.data[14] = 94;
                syntheziser1.data[5] = 175;
                remainderV = 4;
                R = 1;
            } else {

                if ( f < 2199990e3) {
                    syntheziser1.data[13] = 0;
                    syntheziser1.data[14] = 94;
                    syntheziser1.data[5] = 159;
                    remainderV = 2;
                    R = 1;
                } else {
                    syntheziser1.data[13] = 0;
                    syntheziser1.data[14] = 94;
                    syntheziser1.data[5] = 143;
                    remainderV = 1;
                    R = 1;
                }
            }
        }

        break;
    }
    default:
        emit error("ВЫбранна недопустимая сетка частот");
    }

    int intV = floor( remainderV * R * (f / 25e6));
    int fracV = round( moduleV * (remainderV * R * (f / 25e6) - intV));

    syntheziser1.data[20] = (((intV >> 1) >> 8 ) & 0xff);
    syntheziser1.data[21] = ((intV >> 1) & 0xff);
    syntheziser1.data[22] = ((((fracV << 3) >> 8) & 0xff)  & 127);

    if ((intV & 0xff & 1) == 1)
        syntheziser1.data[22] += 128;

    syntheziser1.data[23] = ((fracV & 0xff) << 3);


    serialPort.write((char *)&syntheziser1, sizeof(syntheziser1));
    bool status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "First syntheziser buffer: ";
       for (uint i = 0; i < sizeof(syntheziser1.data); ++i)
        qDebug() << syntheziser1.data[i];
    #endif

    if (!status) {
        //emit error("Не удалось установить частоту");
        return false;
    }

//    bool synth2Changed1 = ((syntheziser2.data[15] == 66) && (switcher.value == 1));
//    bool synth2Changed2 = ((syntheziser2.data[15] == 98) && (switcher.value == 7));
//    if (synth2Changed1 || synth2Changed2) {
    if (switcher.value == 1)
        syntheziser2.data[15] = 98;
    else
        syntheziser2.data[15] = 66;


    serialPort.write((char *)&syntheziser2, sizeof(syntheziser2));
    status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "Second syntheziser buffer: ";
       for (uint i = 0; i < sizeof(syntheziser2.data); ++i)
        qDebug() << syntheziser2.data[i];
    #endif

        //if (!status)
            //emit error("Не удалось установить частоту");

//    }

    printMessage("Setted Frequency " + QString::number(f) + "Hz");

    double amp = currentAmp;
    setAmp(amp);
    emit newAmplitude(amp);
    return true;
}





void G3000 :: setFrequencyGrid(int i_frequencyGrid)
{
    // Предусловия

    switch (i_frequencyGrid)
    {
    case Grid1:
    {
        frequencyGrid = Grid1;
        break;
    }
    case Grid2:
    {
        frequencyGrid = Grid2;
        break;
    }
    case Grid5:
    {
       frequencyGrid = Grid5;
       break;
    }
    case Grid10:
    {
        frequencyGrid = Grid10;
        break;
    }
    default:
        emit error("Выбранна недопустимая сетка частот");
    }
}

FrequencyGrid G3000::getFrequencyGrid()
{
    return frequencyGrid;
}

double G3000::getReferenceFrequency(int refFreq)
{
    switch (refFreq)
    {
    case RefFreq1100:
    {
        return 1100e6;
        break;
    }
    case RefFreq1150:
    {
        return 1150e6;
        break;
    }
    default:
        emit error("Unknown referance frequency");
        return NAN;
    }
}

void G3000::setLevelControlMode(LevelControlMode mode)
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

LevelControlMode G3000::getLevelControlMode()
{
    return levelControlMode;
}
