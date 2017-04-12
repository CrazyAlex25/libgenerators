#ifndef G3000_H
#define G3000_H

#include <QObject>
#include <QString>
#include <QDebug>
#include <QtSerialPort/QSerialPortInfo>
#include <QtSerialPort/QSerialPort>
#include <QThread>
#include <G3000/commands.h>
#include <generators_global.h>

/* Класс G3000 осуществляет управление генераторами РАДИЙ-ТН
 *
 * Стратегия обработки ошибок: при возникновении ошибки при управлении генератором
 * излучается соответстующий сигнал. Дополнительная информация о работе доступна при запуске через
 * консоль
 */
class GENERATORS_EXPORT G3000 : public QObject
{
    Q_OBJECT
public:
    explicit G3000(QObject * parent = 0);

    bool turnOn(bool i_on);
    bool GENERATORS_EXPORT setAmp(float amp);
    bool GENERATORS_EXPORT setFrequency(float f);
    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid);
    bool GENERATORS_EXPORT connect();

    //возможные сетки частот генератора
    enum eFrequencyGrid {
        Grid1, // 1 Кгц
        Grid2, // 2 КГц
        Grid5, // 5 КГц
        Grid10 // 10 КГц
    };

signals:
    void error(QString e);

private:

    bool commute(quint8);
    bool checkResponse();
    float roundToGrid(float);
    float getReferenceFrequency(int refFreq);

    // Определение опорных частот
    enum eReferenceFrequency{
        RefFreq1100,  // 1100 МГЦ
        RefFreq1150,  // 1150 МГЦ
        UnknownRefFreq
    };

    // Состояния коммутатора
    enum eSwitcherState{
        DirectSignal,
        DiffSignal
    };

    bool on;


    float referenceFrequency;
    float lowestFrequency;
    float highestFrequency;
    int frequencyGrid;

    //quint8 commutator;

    Response response;
    Syntheziser syntheziser1;
    Syntheziser syntheziser2;
    Attenuator attenuator1;
    Attenuator attenuator2;
    Switcher switcher;



    QSerialPort serialPort;

};

#endif // G3000_H
