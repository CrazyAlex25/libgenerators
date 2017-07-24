#include "calibrator.h"
#include <QDir>
#include <QTextStream>
#include <QtGlobal>
#include <QDebug>

Calibrator::Calibrator(QObject *parent) : QObject(parent),
    stepHigh(NAN),
    stepLow(NAN),
    ampMax(NAN),
    bandBorder(NAN)
{

}

void Calibrator:: load(QString fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        emit error("Не найден файл грубой калибровки");

    QTextStream in(&file);
    double f1;
    double f2;
    double P_dBm;
    double amp_V;
    float ampMax = -qInf();
    in >> f1;
    in >> P_dBm;
    amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
    ampCorrection.push_back(amp_V);

    if (amp_V > ampMax)
        ampMax = amp_V;

    in >> f2;
    in >> P_dBm;
    amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
    ampCorrection.push_back(amp_V);

    if (amp_V > ampMax)
        ampMax = amp_V;

    stepLow = (f2 - f1) * 1e6;
    f1 = f2;

    double *stepPtr = &stepLow;

    while (!in.atEnd())
    {
        double step;
        in >> f2;
        in >> P_dBm;

        bool eof = ( f2 == 0) && (P_dBm == 0);
        if (eof)
            break;

        amp_V = pow(10, ((P_dBm + 30 + 16.99)/ 20) - 3) * sqrt(2);
        ampCorrection.push_back(amp_V);

        if (amp_V > ampMax)
            ampMax = amp_V;

        step = (f2 - f1) * 1e6;
        if ( std::fabs((step - *stepPtr)) > 60e3) {


            bool err = (f2 * 1e6 < bandBorder ) || (f1 * 1e6 > bandBorder);
            if (err)
                emit error("Ошибка при загрузки калибровочной характиристики. Не правильная структура файла");

            stepHigh = step;
            stepPtr = &stepHigh;
        }
        f1= f2;
    }

    qDebug() << "Frequency step in low band is: " + QString::number(stepLow);
    qDebug() << "Frequency step in high band is: " + QString::number(stepHigh);
}

void Calibrator::setBandBorder(float fHz)
{
    bandBorder = fHz;
}

double Calibrator::getAmp(float f)
{
    if (std::isnan(f))
        return ampCorrection[1];

    int ind;
    if ( f < bandBorder) {
        ind = round(f / stepLow) + 1 ;
    } else {
        ind = round(bandBorder / stepLow) + round((f - bandBorder) / stepHigh) + 1;
    }

    return ampCorrection[ind];
}
