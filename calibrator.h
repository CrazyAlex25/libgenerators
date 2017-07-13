#ifndef CALIBRATOR_H
#define CALIBRATOR_H

#include <QObject>

class Calibrator : public QObject
{
    Q_OBJECT
public:
    explicit Calibrator(QObject *parent = nullptr);

signals:

public slots:
};

#endif // CALIBRATOR_H