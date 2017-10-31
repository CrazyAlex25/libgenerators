#ifndef G6009_H
#define G6009_H

#include <QObject>
#include <generator.h>
#include <generators_global.h>
#include <G6009/commands6009.h>

class  GENERATORS_EXPORT  G6009 : public Generator
{
    Q_OBJECT
public:
    explicit G6009(QObject * parent = 0);
    ~G6009();

public slots:
    bool GENERATORS_EXPORT turnOn(bool i_on) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setAmp(float &m_amp) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setFrequency(float &m_f) Q_DECL_OVERRIDE;

public:
    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid) Q_DECL_OVERRIDE;
    FrequencyGrid GENERATORS_EXPORT getFrequencyGrid() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT connect(QSerialPortInfo &) Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setLevelControlMode(LevelControlMode mode) Q_DECL_OVERRIDE;
    LevelControlMode GENERATORS_EXPORT getLevelControlMode() Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setSynthLevel(int level);
    bool GENERATORS_EXPORT isG6009(QSerialPortInfo &info);

    // Состояния коммутатора
    enum eSwitcherState{
        LowFrequency,
        HighFrequency
    };


    enum eSynthLevel{
        Off,
        SynthLevelM4,
        SynthLevelM1,
        SynthLevelP2,
        SynthLevelP5,
    };

signals:
//    void error(QString e);
//    void disconnected();
//    void newFrequency(float freq_Hz);
//    void newAmplitude(float amp_V);
//    void newTFm(float t_s);

private:

    bool  commute(quint8);
    bool setAttenuation(float &attenuation);
    float getAttenuation();
    bool checkResponse();
    float getReferenceFrequency(int refFreq);
    double getAmpCorrection();
    void writeHead();


    // Определение опорных частот
    enum eReferenceFrequency{
//        RefFreq1100,  // 1100 МГЦ
//        RefFreq1150,  // 1150 МГЦ
        UnknownRefFreq
    };

    quint8 synthLevel;
    float referenceFrequency;

    float attenuationMax;
    float attenuationMin;
    float attenuationStep;

    Head6009 head;
    Response6009 response;
    Syntheziser6009 syntheziser;
    Dds6009  dds;
    Attenuator6009 attenuator1;
    Attenuator6009 attenuator2;
    Switcher6009 switcher;
    Reset6009 reset;

};

#endif // G6009_H
