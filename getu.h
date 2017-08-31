#ifndef GETU_H
#define GETU_H

#include "G3000/g3000.h"
#include "generators_global.h"

class GENERATORS_EXPORT GetU : public G3000
{
    Q_OBJECT
public:
    explicit GetU(QObject * parent = 0);

    void timerEvent(QTimerEvent *event) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT turnOn(bool i_on) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setAmp(float &m_amp) Q_DECL_OVERRIDE;
    bool GENERATORS_EXPORT setFrequency(float &m_f) Q_DECL_OVERRIDE;

    bool GENERATORS_EXPORT isGetU(QSerialPortInfo &info);

private:
    int timeOutTimerId;
    int poolPeriod;
    float freq;

};

#endif // GETU_H
