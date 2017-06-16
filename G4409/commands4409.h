#ifndef COMMANDS4409
#define COMMANDS4409

#include <generators_global.h>
#include <QtGlobal>

struct GENERATORS_EXPORT Head4409
{
   quint8 data[5] = { 0xA5, 0xA4, 0xA3, 0xA2, 0xA1};
};


struct GENERATORS_EXPORT Reset4409
{
    Head4409 head;
    quint8 data = 0xA0;
};

struct GENERATORS_EXPORT Attenuator4409
{
    Head4409 head;
    quint8 command = 0xA1;
    quint8 id = 0x00;
    quint8 data = 0x00;
};

struct GENERATORS_EXPORT Syntheziser4409
{
    Head4409 head;
    quint8 command = 0xA2;
    quint8 id = 0x01;
    quint8 data[20] = {0x01, 0x40, 0x00, 0x05,
                       0x61, 0x00, 0x41, 0xFC,
                       0x00, 0x00, 0x00, 0x03,
                       0x00, 0x00, 0x7E, 0x42,
                       0x60, 0x00, 0xCE, 0x21};
    quint8 freq[4] = {0x00, 0x00, 0x00, 0x00};
};

struct GENERATORS_EXPORT Dds4409
{
    Head4409 head;
    quint8 command = 0xA2;
    quint8 id = 0x02;
    quint8 phase =  {0x00};
    quint8 freq[4] = {0x00, 0x00, 0x00, 0x00};
};


struct GENERATORS_EXPORT Switcher4409
{
    Head4409 head;
    quint8 command = 0xA4;
    quint8 value = 0x00;
};


struct GENERATORS_EXPORT Response4409
{
    quint8 data[5] = {0xA4, 0xA3, 0xA2, 0xA1, 0xA0};
};


#endif // COMMANDS4409

