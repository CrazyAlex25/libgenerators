#ifndef GENERATOR_H
#define GENERATOR_H

#include <QObject>
#include <generators_global.h>
#include <QtSerialPort/QSerialPortInfo>
#include <QtSerialPort/QSerialPort>
#include <QTime>
#include <QFile>
#include <QVector>


typedef int FrequencyGrid;
typedef int LevelControlMode;

/* Класс Generator является базовым для всех USB генераторов компании РАДИЙ ТН и
 * и определяет интерфейс производных классов
 *
* Все велечины передаются в системе Cи ( Гц, В, сек).
* Стратегия обработки ошибок: при возникновении ошибки при управлении генератором
* излучается соответстующий сигнал. Дополнительная информация о работе возможна при запуске через
* консоль, если ее разрешить вызовом метода enableVerbose(true). Так же вся информация сохраняется
* в файле log.txt, которые находится в одной папке с испольняемым файлом
*/

class Generator : public QObject
{
  Q_OBJECT
public:
    explicit Generator(int i_vid, int i_pid, float i_lowestFreq, float i_highestFreq, float i_tSweepMin, float i_tSweepMax, QObject * parent = 0);

    //Включение генератора
    virtual bool GENERATORS_EXPORT turnOn(bool i_on) = 0;

    // Установка амплитуды
    virtual bool GENERATORS_EXPORT setAmp(float &m_amp) = 0;
    // Возврат текущего значения амплитуды
    virtual float GENERATORS_EXPORT getAmp() = 0;

    // Установка частоты
    virtual bool GENERATORS_EXPORT setFrequency(float &m_f) = 0;
    // Возврат текущего значения частоты
    virtual float GENERATORS_EXPORT getFrequency() = 0;

    // Запуск качания частоты
    virtual bool GENERATORS_EXPORT startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode) = 0;
    // Остановка качания частоты
    virtual void GENERATORS_EXPORT stopFrequencySweep() = 0;

    // Установка шага частотной сетки
    virtual void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid) = 0;
    // Возврат шага частотной сетки
    virtual FrequencyGrid GENERATORS_EXPORT getFrequencyGrid() = 0;

    //Установить связь с устройством с заданным pid и vid ( работает только если нет устройств
    // с одинаковыми pid и vid)
    virtual bool GENERATORS_EXPORT autoconnect() = 0;
    //Установить связь с заданным устройством
    virtual bool GENERATORS_EXPORT connect(QSerialPortInfo *) = 0;

    // Получить список доступных устройств
    virtual QList<QSerialPortInfo> GENERATORS_EXPORT getAvailablePorts() = 0;
    // Получить информацию о текущем порте
    virtual QSerialPortInfo GENERATORS_EXPORT getPortInfo() = 0;

    // Включить вывод информации в консоль
    void GENERATORS_EXPORT enableVerbose(bool);
    // Включить логи
    void GENERATORS_EXPORT enableLogs(bool);

    // Переключение режима управление сигнала (по умолчанию стоит режим управления амплитудой)
    virtual void GENERATORS_EXPORT setLevelControlMode(LevelControlMode mode) = 0;
    // Возврат текущего режима управления сигналом генератора
    virtual int GENERATORS_EXPORT getLevelControlMode() = 0;

    //возможные сетки частот генератора
    enum eFrequencyGrid {
        Grid1, // 1 Кгц
        Grid2, // 2 КГц
        Grid5, // 5 КГц
        Grid10 // 10 КГц
    };

    // режимы качания частоты
    enum eSweepMode{
        SweepToHigh, // от меньшей к большей
        SweepToLow   // от большей к меньше
    };

    // режимы управления уровнем сигнала генератора
    enum eLevelControlMode{
        Amplitude,
        Attenuation
    };

signals:
    void error(QString e);
    void disconnected();
    void newFrequency(float freq_Hz);
    void newAmplitude(float amp_V);
    void newTSweep(float t_s);

protected:

    float roundToGrid(float);
     void  printMessage(QString message);

    const int vid;
    const int pid;

    bool on;
    bool connected;

    bool verbose;
    bool log;

    const float lowestFrequency;
    const float highestFrequency;
    int frequencyGrid;
    float currentFrequency;

    float currentAmp;
    float ampMax;

    float fSweepStart;
    float fSweepStop;
    float fSweepStep;
    float fSweep;
    int sweepMode;

    const float tSweepMin;
    const float tSweepMax;
    QTime tSweepStart;

    int levelControlMode;

    QVector<double> ampCorrection;
    float fAmpCorrectionStep;

    int connectionTimerId;
    int freqSweepTimerId;

    QSerialPort serialPort;
    QSerialPortInfo *serialPortInfo;

    QString logFileName;
    QFile logFile;

};
#endif // INTERFACE_H
