#include "generator.h"
#include <QDir>
#include <QTextStream>
#include <QDebug>

Generator::Generator(int i_vid, int i_pid, float i_lowestFreq, float i_highestFreq, float i_tSweepMin, float i_tSweepMax, QObject *parent) : QObject(parent),
    vid(i_vid),
    pid(i_pid),
    on(false),
    connected(false),
    verbose(false),
    log(true),
    lowestFrequency(i_lowestFreq),
    highestFrequency(i_highestFreq),
    currentFrequency(NAN),
    currentAmp(0),
    fSweepStart(NAN),
    fSweepStop(NAN),
    fSweepStep(NAN),
    fSweep(NAN),
    sweepMode(SweepToHigh),
    tSweepMin(i_tSweepMin),
    tSweepMax(i_tSweepMax),
    levelControlMode(-1),
    fAmpCorrectionStep(NAN),
    freqSweepTimerId(-1),
    serialPortInfo(NULL),
    logFileName(QDir::currentPath()+"/log.txt")
{

    QFile logFile(logFileName);
    if (logFile.open(QFile::WriteOnly)) {
        QTextStream logStream(&logFile);
        logStream << QDateTime::currentDateTime().toString() << "\n";
    }
    logFile.close();

}

void Generator::printMessage(QString message)
{
    if (verbose)
        qDebug() << message;

    if (log) {
        QFile logFile(logFileName);
        if (logFile.open(QFile::Append | QFile::Text)) {
            QTextStream logStream(&logFile);
            logStream  << "\n" << message  ;
        }
        logFile.close();
    }

}

void Generator::enableVerbose(bool input)
{
    verbose = input;
}

void Generator::enableLogs(bool input)
{
    log = input;
}

float Generator::roundToGrid(float f)
{

    float result = 0;

    switch (frequencyGrid)
    {
    case Grid1:
    {
        result = 1e3 * round( f / 1e3);
        break;
    }
    case Grid2:
    {
        result = 2e3 * round( f / 2e3);
        break;
    }
    case Grid5:
    {
        result = 5e3 * round( f / 5e3);
        break;
    }
    case Grid10:
    {
        result = 10e3 * round( f / 10e3);
        break;
    }
    default:
        emit error("Выбранна недопустимая сетка частот");
    }

    return result;
}

