#ifndef G3000_H
#define G3000_H

#include <generator.h>
#include <QThread>
#include <G3000/commands3000.h>
#include <cmath>

#include <QVector>


/* Класс G3000 осуществляет управление генератором РГШ3000
 */

class GENERATORS_EXPORT G3000 : public Generator
{
    Q_OBJECT
public:
    explicit G3000(QObject * parent = 0);
    explicit G3000(int i_vid, int i_pid, QObject *parent = 0);
    ~G3000();

public slots:
    bool GENERATORS_EXPORT turnOn(bool i_on) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setAmp(double &m_amp) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setFrequency(double &m_f) Q_DECL_OVERRIDE;

public:
    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid) Q_DECL_OVERRIDE;
    FrequencyGrid GENERATORS_EXPORT getFrequencyGrid() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT connect(QSerialPortInfo & info) Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setLevelControlMode(LevelControlMode mode) Q_DECL_OVERRIDE;
    LevelControlMode GENERATORS_EXPORT getLevelControlMode() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT isG3000(QSerialPortInfo &info);
signals:
//    void error(QString e);
//    void disconnected();
//    void newFrequency(double freq_Hz);
//    void newAmplitude(double amp_V);
//    void newTFm(double t_s);

protected:

    void initialize();
    bool setAttenuation(double &attenuation);
    double getAttenuation();

    bool commute(quint8);
    bool checkResponse();
    double getReferenceFrequency(int refFreq);

private:

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

    double referenceFrequency;

    double attenuationMax;
    double attenuationMin;
    double attenuationStep;

    Head3000 head;
    Response3000 response;
    Syntheziser3000 syntheziser1;
    Syntheziser3000 syntheziser2;
    Attenuator3000 attenuator1;
    Attenuator3000 attenuator2;
    Switcher3000 switcher;

};

#endif // G3000_H
