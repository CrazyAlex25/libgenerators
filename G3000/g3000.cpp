#include "G3000/g3000.h"

G3000::G3000(QObject *parent) : QObject(parent),
    on(false),
    referenceFrequency(UnknownRefFreq),
    lowestFrequency(1e6),
    highestFrequency(4.4e9)
{
    setFrequencyGrid(Grid10);
    syntheziser1.data[7] = 36;
    //commutator = 0;

    syntheziser1.id[0] = 0x31;
    syntheziser1.id[1] = 0x31;

    syntheziser2.id[0] = 0x31;
    syntheziser2.id[1] = 0x32;

}


bool G3000::connect()
{
    QList<QSerialPortInfo> info = QSerialPortInfo::availablePorts();

    if (info.isEmpty()) {
        emit error("Couldn't find generator");
        return false;
    }

    if (info.length() > 1) {
       emit error("More than 1 serial ports available");
        return false;
    }

    serialPort.setPort(info[0]);
    bool ok = serialPort.open(QIODevice::ReadWrite);
    if (!ok) {
        emit error("Enable to open serial port. " + serialPort.errorString());
        return false;
    }


    ok = serialPort.setBaudRate(QSerialPort::Baud115200);
    if (!ok)
    {
        emit error("Couldn't set baud rate");
        return false;
    }

    ok = serialPort.setDataBits(QSerialPort::Data8);

    if (!ok)
    {
        emit error("Couldn't set data bits");
        return false;
    }


    ok = serialPort.setParity(QSerialPort::NoParity);

    if (!ok)
    {
        emit error("Couldn't set parity");
        return false;
    }

    return true;
}

/* Включение/выключение генератора*/
bool G3000::turnOn(bool i_on)
{
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
       qDebug() << "буффер включения: ";
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

bool G3000::setAmp(float amp)
{
    float Nrm = 0;
    attenuator1.data = (quint8) ((amp + Nrm) / 2);
    attenuator2.data = ((quint8) (amp + Nrm)) - attenuator1.data;

    //float V = (attenuation[0]  + attenuation[1]) / 2;
    attenuator2.data += 128;

    serialPort.write((char *)&attenuator1, sizeof(attenuator1) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "буффер аттениютора: ";
       for (uint i = 0; i < sizeof(attenuator1.data); ++i)
        qDebug() << attenuator1.data;
    #endif


    bool success;
    success = checkResponse();
    if (!success) {
        emit error ("Couldn't set amplitude");
        return false;
 }


    serialPort.write((char *)&attenuator2, sizeof(attenuator2) / sizeof(char));

    #ifdef QT_DEBUG
        qDebug() << "буффер аттениютора: ";
       for (uint i = 0; i < sizeof(attenuator2.data); ++i)
        qDebug() << attenuator2.data;
    #endif
    success = checkResponse();
    if (!success) {
        emit error ("Couldn't set amplitude");
        return false;
    }

    return true;
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
        qDebug() << "Ответ от генератора не получен";
        return false;
    }

    // Считываем ответ
    char responseFromGen[sizeof(response) + 1] = {0};
    this->thread()->usleep(20);

    qint64 bytesToRead = serialPort.read(responseFromGen, sizeof(responseFromGen));


    if (bytesToRead == -1) {
        qDebug() << "Невозможно считать данные с последовательного порта";
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

/* Установка частоты */
bool G3000 :: setFrequency(float f)
{
    syntheziser1.data[15] = 0x42;

    // Обработка входных данных
    if ( f < lowestFrequency)
        f = lowestFrequency;
    if ( f > highestFrequency )
        f = highestFrequency;

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

    // Округление до деления выбранной сетки
    f = roundToGrid(f);

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

    int intV = round( remainderV * R * f / 25e6);
    int fracV = round( moduleV * (remainderV * R * f / 25e6 - intV));

    qDebug()<< "syntheziser1[28] =" << (((intV >> 1) >> 8 ) & 0xff);
    qDebug()<< "syntheziser1[29] =" << ((intV >> 1) & 0xff);
    qDebug()<< "syntheziser1[30] =" << ((((fracV << 3) >> 8) & 0xff)  & 127);

    syntheziser1.data[20] = (((intV >> 1) >> 8 ) & 0xff);
    syntheziser1.data[21] = ((intV >> 1) & 0xff);
    syntheziser1.data[22] = ((((fracV << 3) >> 8) & 0xff)  & 127);

    if ((intV & 0xff & 1) == 1)
        syntheziser1.data[22] += 128;

    syntheziser1.data[22] = ((fracV & 0xff) >> 3);


    serialPort.write((char *)&syntheziser1, sizeof(syntheziser1));
    bool status = checkResponse();

    #ifdef QT_DEBUG
        qDebug() << "буффер 1 синтезатора: ";
       for (uint i = 0; i < sizeof(syntheziser1.data); ++i)
        qDebug() << syntheziser1.data[i];
    #endif

    if (!status) {
        emit error("Не удалось установить частоту");
        return false;
    }

    bool synth2Changed1 = ((syntheziser2.data[15] == 66) && (switcher.value == 1));
    bool synth2Changed2 = ((syntheziser2.data[15] == 98) && (switcher.value == 7));
    if (synth2Changed1 || synth2Changed2) {
        if (switcher.value == 1)
            syntheziser2.data[15] = 98;
        else
            syntheziser2.data[15] = 66;


        serialPort.write((char *)&syntheziser2, sizeof(syntheziser2));
        status = checkResponse();

        #ifdef QT_DEBUG
            qDebug() << "буффер 2 синтезатора: ";
           for (uint i = 0; i < sizeof(syntheziser2.data); ++i)
            qDebug() << syntheziser2.data[i];
        #endif

        if (!status)
            emit error("Не удалось установить частоту");

    }

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
        emit error("ВЫбранна недопустимая сетка частот");
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
