#include "G3000/g3000.h"

G3000::G3000(QObject *parent) : QObject(parent),
    vid(0x0403),
    pid(0x6001),
    on(false),
    connected(false),
    referenceFrequency(UnknownRefFreq),
    lowestFrequency(1e6),
    highestFrequency(3.0e9),
    currentFrequency(NAN),
    currentAmp(0),
    fSweepStart(NAN),
    fSweepStop(NAN),
    fSweepStep(NAN),
    fSweep(NAN),
    sweepMode(SweepToHigh),
    tSweepMin(0.020),
    tSweepMax(1000),
    freqSweepTimerId(-1),
    serialPortInfo(NULL)
{
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
    if (connected)
        delete serialPortInfo;
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
            if (info[i].productIdentifier() == serialPortInfo->productIdentifier())
                isInList = true;

        if (info.isEmpty() || !isInList ) {
            qDebug() << "Lost connection";
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

        setFrequency(fSweep);
        qDebug()<<"Setted Frequency" + QString::number(fSweep) + "Hz";
        emit frequencySweeped(fSweep);
    }

}

bool G3000::connect()
{
    qDebug()<<"Start searching for device";

    int deviceCounter = 0;
    int deviceNum = -1;

    QList<QSerialPortInfo> info = QSerialPortInfo::availablePorts();

    if (info.isEmpty()) {
        qDebug()<<"Can't find generator";
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
       qDebug()<<"Can't find generator";
       return false;
    }

    if (deviceCounter > 1) {
       emit error("Найдено более одного устройства");
        return false;
    }

    this->thread()->sleep(1);

    //Обновляем информации о порте
    serialPortInfo = new QSerialPortInfo(info[deviceNum]);
    serialPort.setPort(*serialPortInfo);

    bool ok = serialPort.open(QIODevice::ReadWrite);
    if (!ok) {
        serialPort.close();
        delete serialPortInfo;
        serialPortInfo = NULL;
        //emit error("Не удалось получить доступ к последовательному порту. " + serialPort.errorString());
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
    qDebug() << "Generator has been connected";
    return true;
}

/* Включение/выключение генератора*/
bool G3000::turnOn(bool i_on)
{
    if (!connected) {
        qDebug() << "Can't execute command. Generator is not connected";
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
            qDebug() << "Wrong key for switcher";
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
    if (!connected) {
        qDebug() << "Can't execute command. Generator is not connected.";
        return false;
    }

    currentAmp = amp;

    float Nrm = 0;
    attenuator1.data = (quint8) ((amp + Nrm) / 2);
    attenuator2.data = ((quint8) (amp + Nrm)) - attenuator1.data;

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

    qDebug()<<"Setted Attenuation  " + QString::number(amp) + "dB.";
    return true;
}

float G3000::getAmp()
{
    return currentAmp;
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
        qDebug() << "No response from generator";
        return false;
    }

    // Считываем ответ
    char responseFromGen[sizeof(response) + 1] = {0};
    this->thread()->usleep(20);

    qint64 bytesToRead = serialPort.read(responseFromGen, sizeof(responseFromGen));


    if (bytesToRead == -1) {
        qDebug() << "Can't read data from searial port";
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

    qDebug()<<"Setted Frequency " + QString::number(f) + "Hz";
    return true;
}

float G3000::getFrequency()
{
    return currentFrequency;
}

bool G3000::startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode)
{
    if (!connected) {
        qDebug() << "Can't execute command. Generator is not connected";
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


    if ((m_fStop >= 275e6) && (m_fStart < 275e6)) {
        emit error("Сканирование возможно в интервалах [1 МГц; 275МГц) и (275 МГц; 3ГГЦ] ");
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

    freqSweepTimerId = startTimer(t_msec);
    setFrequency(fSweep);
    emit frequencySweeped(fSweep);
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

float G3000::roundToGrid(float f)
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
