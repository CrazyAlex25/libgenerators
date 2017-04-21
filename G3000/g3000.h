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
#include <QTimerEvent>

typedef int FrequencyGrid;

/* Класс G3000 осуществляет управление генераторами РАДИЙ-ТН
 *
 * Все велечины передаются в системе Cи. Гц, В, сек.
 * Стратегия обработки ошибок: при возникновении ошибки при управлении генератором
 * излучается соответстующий сигнал. Дополнительная информация о работе доступна при запуске через
 * консоль
 */
class GENERATORS_EXPORT G3000 : public QObject
{
    Q_OBJECT
public:
    explicit G3000(QObject * parent = 0);
    ~G3000();

    bool turnOn(bool i_on);
    bool GENERATORS_EXPORT setAmp(float &m_amp);
    float GENERATORS_EXPORT getAmp();

    bool GENERATORS_EXPORT setFrequency(float &m_f);
    float GENERATORS_EXPORT getFrequency();

    bool GENERATORS_EXPORT startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode);
    void GENERATORS_EXPORT stopFrequencySweep();

    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid);
    FrequencyGrid GENERATORS_EXPORT getFrequencyGrid();

    bool GENERATORS_EXPORT connect();

    //возможные сетки частот генератора
    enum eFrequencyGrid {
        Grid1, // 1 Кгц
        Grid2, // 2 КГц
        Grid5, // 5 КГц
        Grid10 // 10 КГц
    };

    enum eSweepMode{
        SweepToHigh,
        SweepToLow
    };

signals:
    void error(QString e);
    void disconnected();
    void frequencySweeped(float freq_Hz);

private:

    void timerEvent(QTimerEvent *event);
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
    float currentFrequency;

    float currentAmp;

    float fSweepStart;
    float fSweepStop;
    float fSweepStep;
    float fSweep;
    int sweepMode;

    float tSweepMin;
    float tSweepMax;

    int connectionTimerId;
    int freqSweepTimerId;

    Response response;
    Syntheziser syntheziser1;
    Syntheziser syntheziser2;
    Attenuator attenuator1;
    Attenuator attenuator2;
    Switcher switcher;



    QSerialPort serialPort;
    QSerialPortInfo *serialPortInfo;

};

#endif // G3000_H
