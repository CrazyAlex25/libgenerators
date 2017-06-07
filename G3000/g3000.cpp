#include "G3000/g3000.h"
#include <QDebug>

G3000::G3000(QObject *parent) :
    Generator(0x0403, //vid
                                          0x6001, //pid
                                          1e6, // lowestFreq
                                          3e9, // highestFreq
                                          0.020, // tSweepMin
                                          1000, // tSweepMax
                                          parent),
    referenceFrequency(UnknownRefFreq),
    attenuationMax(63),
    attenuationMin(0),
    attenuationStep(0.5)
{

    Q_INIT_RESOURCE(amp);


    setFrequencyGrid(Grid10);
    syntheziser1.data[7] = 36;
    //commutator = 0;

    syntheziser1.id[0] = 0x31;
    syntheziser1.id[1] = 0x31;

    syntheziser2.id[0] = 0x31;
    syntheziser2.id[1] = 0x32;

    connectionTimerId = startTimer(500);
}

G3000::~G3000()
{
    if (connected) {
        turnOn(false);
        delete serialPortInfo;
    }

  //  logFile.close();
}

/* проверяем связь с генератором */
void G3000::timerEvent(QTimerEvent * event)
{
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
    if  (event->timerId() == freqSweepTimerId) {

        switch (sweepMode) {
        case SweepToHigh:
            fSweep += fSweepStep;

            if ((fSweep >= fSweepStop))
                fSweep = fSweepStart;
            break;

        case SweepToLow:
            fSweep -= fSweepStep;

            if ((fSweep <= fSweepStart))
                fSweep = fSweepStop;
            break;
        default:
            break;
        }

        QTime tSweepStop = QTime::currentTime();
        float tSweep =  tSweepStart.msecsTo(tSweepStop) * 1e-3;
        emit newTSweep(tSweep);
        setFrequency(fSweep);

        printMessage( "Setted Frequency" + QString::number(fSweep) + "Hz");

        emit newFrequency(fSweep);
    }

}

bool G3000::autoconnect()
{
    printMessage("Start searching for device");

    int deviceCounter = 0;
    int deviceNum = -1;

    QList<QSerialPortInfo> info = QSerialPortInfo::availablePorts();

    if (info.isEmpty()) {

        printMessage( "Can't find generator");

        return false;
    }

    for (int i = 0; i < info.length(); ++i)
    {

        if ((info[i].vendorIdentifier() == vid) && (info[i].productIdentifier() == pid)) {
            deviceNum = i;
            ++deviceCounter;
        }

    }


    if (deviceCounter == 0) {
      printMessage("Can't find generator");
       return false;
    }

    if (deviceCounter > 1) {
       emit error("Найдено более одного устройства");
        return false;
    }

    this->thread()->sleep(1);

    return connect(&info[deviceNum]);
}

bool G3000::connect(QSerialPortInfo *info)
{
    //Обновляем информации о порте
    serialPortInfo = new QSerialPortInfo(*info);
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

    connected = true;

    printMessage( "Generator has been connected");


    loadCalibrationAmp();

    printMessage("Amp calibration loaded");


     return true;
}

QList<QSerialPortInfo> G3000::getAvailablePorts()
{
    return QSerialPortInfo::availablePorts();
}

QSerialPortInfo G3000::getPortInfo()
{
    return *serialPortInfo;
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
        //emit error ("Возникла ошибка при включении");
        return false;
    }

    return true;
}

// Установка амлитуды. Функция возвращает значение реально установленной амплитуды
bool G3000::setAmp(float &amp)
{
    currentAmp = amp;
    bool success;

    /* В зависимости от режима управления уровнем сигнала либо идет пересчет
     * амплитуды в ослабление аттенюатора, либо нет.
     */
    switch (levelControlMode)
    {
    case Amplitude:
    {
         double maxAmp = getAmpCorrection();

         if (amp > maxAmp)
             amp = maxAmp;

         float attenuation = 20 * log10(maxAmp / amp);

         success = setAttenuation(attenuation);

         amp = maxAmp / pow(10, attenuation / 20);


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
bool G3000::setAttenuation(float &attenuation)
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

    //float V = (attenuation[0]  + attenuation[1]) / 2;
    attenuator2.data += 128;

    serialPort.write((char *)&attenuator1, sizeof(attenuator1) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "Attenuation buffer: ";
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
        qDebug() << "atttenuation buffer: ";
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

float G3000::getAmp()
{
    return currentAmp;
}

double G3000::getAmpCorrection()
{
    if (std::isnan(currentFrequency)) {
        return ampCorrection[1];
    } else {
        int ind = round(currentFrequency / fAmpCorrectionStep);
        return ampCorrection[ind];
    }

}

void G3000::loadCalibrationAmp()
{

    QFile file(":/calibration.txt");
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

/* Метод проверяет ответ от генератора. Возращает true, если получен
 * ожидаемый ответ.
 */
bool G3000::checkResponse()
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
bool G3000 :: setFrequency(float &m_freq)
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
    float f = m_freq;

    // Нормировка амплитуды


    // Выставление опорного генератора
    if (f < 275e6) {
        //float referenceFrequency_old = referenceFrequency;
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

    float amp = currentAmp;
    setAmp(amp);
    emit newAmplitude(amp);
    return true;
}

float G3000::getFrequency()
{
    return currentFrequency;
}

bool G3000::startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode)
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

    if (m_timeStep < tSweepMin)
        m_timeStep = tSweepMin;

    if (m_timeStep > tSweepMax)
        m_timeStep = tSweepMax;

    int t_msec = round(m_timeStep * 1e3);
    m_timeStep = t_msec / 1e3;

    fSweepStep = m_fStep;
    fSweepStart = m_fStart;
    fSweepStop = m_fStop;

    if (m_fStart > m_fStop) {
        float tmp = m_fStart;
        m_fStart = m_fStop;
        m_fStop = tmp;
    }


    if ((m_fStop > 275e6) && (m_fStart <= 275e6)) {
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

    fSweepStart = m_fStart;
    fSweepStop = m_fStop;
    fSweep = fSweepStart;

    switch (i_sweepMode)
    {
    case SweepToHigh:
        sweepMode = SweepToHigh;
        break;

    case SweepToLow:
        sweepMode = SweepToLow;
        break;

    default:
        emit error("Неправильный режим перестройки частоты");
        return false;
        break;
    }

    //  Поиск минимальной амплитуды в полосе
    float ampMin = ampMax;
    for (quint64 f = fSweepStart; f < fSweepStop; f += fSweepStep)
    {
        int ind = f / fAmpCorrectionStep;

        if (ampMin > ampCorrection[ind])
            ampMin = ampCorrection[ind];
    }

    // ограничиваем амлитуду
    if (currentAmp > ampMin)
        currentAmp = ampMin;


    freqSweepTimerId = startTimer(t_msec);
    tSweepStart = QTime::currentTime();
    setFrequency(fSweep);
    emit newFrequency(fSweep);
    return true;
}

void G3000 :: stopFrequencySweep()
{
    if (freqSweepTimerId != -1)
        killTimer(freqSweepTimerId);

    freqSweepTimerId = -1;
    fSweepStart = NAN;
    fSweepStop = NAN;
    fSweepStep = NAN;
    fSweep = NAN;

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

float G3000::getReferenceFrequency(int refFreq)
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

int G3000::getLevelControlMode()
{
    return levelControlMode;
}
