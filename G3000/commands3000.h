#ifndef COMMANDS3000
#define COMMANDS3000

#include <generators_global.h>
#include <QtGlobal>

struct GENERATORS_EXPORT Head3000
{
   quint8 data[5] = { 0x35, 0x34, 0x33, 0x32, 0x31};
};

struct GENERATORS_EXPORT Syntheziser3000
{
    Head3000 head;
    quint8 id[2] = {0x00, 0x00};
    quint8 data[24] = {0x00, 0x40, 0x00, 0x05,
                       0x00, 0x9f, 0xa4, 0x3c,
                       0x00, 0x00, 0x00, 0x03,
                       0x14, 0x00, 0x5f, 0x62,
                       0x08, 0x00, 0x80, 0x51,
                       0x00, 0x2C, 0x00, 0x00};
};

struct GENERATORS_EXPORT Attenuator3000
{
    Head3000 head;
    quint8 id = 0x32;
    quint8 data = 0x00;
};

struct GENERATORS_EXPORT Switcher3000
{
    Head3000 head;
    quint8 id = 0x33;
    quint8 value = 0x01;
};


struct GENERATORS_EXPORT Response3000
{
    quint8 data[5] = {0x34, 0x33, 0x32, 0x31, 0x30};
};


#endif // COMMANDS3000

