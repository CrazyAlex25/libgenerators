#ifndef G3000_H
#define G3000_H

#include <generator.h>
#include <QThread>
#include <G3000/commands.h>

#include <QTimerEvent>
#include <QVector>

/* Класс G3000 осуществляет управление генератором РГШ3000
 */

class GENERATORS_EXPORT G3000 : public Generator
{
    Q_OBJECT
public:
    explicit G3000(QObject * parent = 0);
    ~G3000();

    bool GENERATORS_EXPORT turnOn(bool i_on) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setAmp(float &m_amp) Q_DECL_OVERRIDE;
    float GENERATORS_EXPORT getAmp() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT setFrequency(float &m_f) Q_DECL_OVERRIDE;
    float GENERATORS_EXPORT getFrequency() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT startFrequencySweep(float &m_fStart, float &m_fStop, float &m_fStep, float &m_timeStep, int i_sweepMode) Q_DECL_OVERRIDE;
    void GENERATORS_EXPORT stopFrequencySweep() Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid) Q_DECL_OVERRIDE;
    FrequencyGrid GENERATORS_EXPORT getFrequencyGrid() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT autoconnect() Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT connect(QSerialPortInfo *) Q_DECL_OVERRIDE;

    QList<QSerialPortInfo> GENERATORS_EXPORT getAvailablePorts() Q_DECL_OVERRIDE;
    QSerialPortInfo GENERATORS_EXPORT getPortInfo() Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setLevelControlMode(LevelControlMode mode) Q_DECL_OVERRIDE;
    int GENERATORS_EXPORT getLevelControlMode() Q_DECL_OVERRIDE;

private:

    bool setAttenuation(float &attenuation);
    float getAttenuation();

    void timerEvent(QTimerEvent *event);
    bool commute(quint8);
    bool checkResponse();
    float getReferenceFrequency(int refFreq);
    void loadCalibrationAmp();
    double getAmpCorrection();


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

    float referenceFrequency;

    float attenuationMax;
    float attenuationMin;
    float attenuationStep;

    Response response;
    Syntheziser syntheziser1;
    Syntheziser syntheziser2;
    Attenuator attenuator1;
    Attenuator attenuator2;
    Switcher switcher;

};

#endif // G3000_H
