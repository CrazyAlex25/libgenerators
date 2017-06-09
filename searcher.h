#ifndef SEARCHER_H
#define SEARCHER_H

#include "G3000/g3000.h"
#include "G4409/g4409.h"

typedef int GeneratorModel;

class Searcher
{
public:
    Searcher();

    //Установить связь с устройством с заданным pid и vid ( работает только если нет устройств
    // с одинаковыми pid и vid)
    static void GENERATORS_EXPORT autosearch(QSerialPortInfo &o_info, GeneratorModel &o_model);
    static void GENERATORS_EXPORT determineModel(QSerialPortInfo i_info, GeneratorModel &o_model);
    enum eModels{
        Generator3000,
        Generator4409,
        None
    };

private:

};

#endif // SEARCHER_H
