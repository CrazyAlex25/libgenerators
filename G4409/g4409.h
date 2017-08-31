#ifndef G4409_H
#define G4409_H

#include <QObject>
#include <generator.h>
#include <generators_global.h>
#include <G4409/commands4409.h>

class  GENERATORS_EXPORT  G4409 : public Generator
{
    Q_OBJECT
public:
    explicit G4409(QObject * parent = 0);
    ~G4409();

    bool GENERATORS_EXPORT turnOn(bool i_on) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setAmp(float &m_amp) Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT setFrequency(float &m_f) Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setFrequencyGrid(int i_frequencyGrid) Q_DECL_OVERRIDE;
    FrequencyGrid GENERATORS_EXPORT getFrequencyGrid() Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT connect(QSerialPortInfo &) Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setLevelControlMode(LevelControlMode mode) Q_DECL_OVERRIDE;
    LevelControlMode GENERATORS_EXPORT getLevelControlMode() Q_DECL_OVERRIDE;

    void GENERATORS_EXPORT setSynthLevel(int level);
    bool GENERATORS_EXPORT isG4409(QSerialPortInfo &info);

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

    Head4409 head;
    Response4409 response;
    Syntheziser4409 syntheziser;
    Dds4409  dds;
    Attenuator4409 attenuator1;
    Attenuator4409 attenuator2;
    Switcher4409 switcher;
    Reset4409 reset;

};

#endif // G4409_H
