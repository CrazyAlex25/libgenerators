#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <QObject>
#include <generators_global.h>
#include <QDir>
#include <QString>
#include <QVector>
#include <cmath>

class Calibrator : public QObject
{
    Q_OBJECT
public:
    explicit Calibrator(QObject *parent = nullptr);
    void load(QString filename);
    double getAmp(double fHz);
    void setBandBorder(double fHz);

signals:
    void error(QString err);

private:
    QVector<double> ampCorrection;
    double stepHigh;
    double stepLow;
    double ampMax;
    double bandBorder;

};

#endif // CALIBRATOR_H
