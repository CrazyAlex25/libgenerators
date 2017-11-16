#include "searcher.h"

Searcher::Searcher()
{

}
int Searcher::autosearch(QSerialPortInfo &info, GeneratorModel &model)
{
    // Ищет среди подключенных устройств генераторы.
    // возвращает количество устройств, подходящих по параметрам
    G3000 g3000;
    G6009 g6009;
    GetU getU;

    int deviceCounter = 0;
    int deviceNum = -1;

    QList<QSerialPortInfo> infoList = QSerialPortInfo::availablePorts();

    if (infoList.isEmpty()) {
        model = None;
        return 0;
    }

    for (int i = 0; i < infoList.length(); ++i)
    {

        bool isRadiyTn = infoList[i].vendorIdentifier() == g3000.getVid();
        bool isRateos = infoList[i].vendorIdentifier() == getU.getVid();

        bool isG3000 = infoList[i].productIdentifier() == g3000.getPid();
        bool isG6009 = infoList[i].productIdentifier() == g6009.getPid();
        bool isGetU =  infoList[i].productIdentifier() == getU.getPid();

        if ((isRadiyTn || isRateos) && (isG3000 || isG6009 || isGetU)) {
            deviceNum = i;
            ++deviceCounter;
        }

    }


    if (deviceCounter == 0) {
        model = None;
        return 0;
    }

    if (deviceCounter > 1) {
        model = None;
        return deviceCounter;
    }

    //g3000.thread()->sleep(1);

    info = infoList[deviceNum];

    determineModel(info, model);
    return deviceCounter;
}

void Searcher::determineModel(QSerialPortInfo info, GeneratorModel &model)
{

    G3000 g3000;
    g3000.enableVerbose(true);
    G6009 g6009;
    g6009.enableVerbose(true);
    GetU getU;
    getU.enableVerbose(true);

    bool success;

    success = g3000.isG3000(info);

    if (success) {
        model = Generator3000;
        return;
    }

    success = g6009.isG6009(info);

    if (success) {
        model = Generator6009;
        return;
    }

    success = getU.isGetU(info);
    if (success) {
        model = GeterodinU;
        return;
    }

    model = None;
    return;
}
