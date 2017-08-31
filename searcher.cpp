#include "searcher.h"

Searcher::Searcher()
{

}
void Searcher::autosearch(QSerialPortInfo &info, GeneratorModel &model)
{

    G3000 g3000;
    G4409 g4409;

    int deviceCounter = 0;
    int deviceNum = -1;

    QList<QSerialPortInfo> infoList = QSerialPortInfo::availablePorts();

    if (infoList.isEmpty()) {
        model = None;
        return;
    }

    for (int i = 0; i < infoList.length(); ++i)
    {

        bool isRadiyTn = infoList[i].vendorIdentifier() == g3000.vid;
        bool isG3000 = infoList[i].productIdentifier() == g3000.pid;
        bool isG4409 = infoList[i].productIdentifier() == g4409.pid;

        if (isRadiyTn && (isG3000 || isG4409)) {
            deviceNum = i;
            ++deviceCounter;
        }

    }


    if (deviceCounter == 0) {
        model = None;
        return;
    }

    if (deviceCounter > 1) {
        model = None;
        return;
    }

    g3000.thread()->sleep(1);


    info = infoList[deviceNum];

    determineModel(info, model);
}

void Searcher::determineModel(QSerialPortInfo info, GeneratorModel &model)
{

    G3000 g3000;
    g3000.enableVerbose(true);
    G4409 g4409;
    g4409.enableVerbose(true);

    bool success;

    success = g3000.isG3000(info);

    if (success) {
        model = Generator3000;
        return;
    }

    success = g4409.isG4409(info);

    if (success) {
        model = Generator4409;
        return;
    }

    model = None;
    return;
}
