#-------------------------------------------------
#
# Project created by QtCreator 2017-03-27T15:48:48
#
#-------------------------------------------------

QT       += network serialport

QT       -= gui

TARGET = generators
TEMPLATE = lib

DEFINES += GENERATORS_LIBRARY

SOURCES += \
    G3000/g3000.cpp \
    G6009/g6009.cpp \
    calibrator.cpp \
    generator.cpp \
    getu.cpp \
    searcher.cpp \
    server.cpp

HEADERS +=\
    G3000/g3000.h \
    generator.h \
    generators_global.h \
    G3000/commands3000.h \
    calibrator.h \
    getu.h \
    searcher.h \
    server.h \
    G6009/g6009.h \
    G6009/commands6009.h

VERSION = 1.3.4


unix {

    INCLUDEPATH += /usr/include/c++/5.4.0
    target.path = /usr/local/lib/radiy
    INSTALLS += target

    headersGeneral.path = /usr/local/include/radiy/generators
    headersGeneral.files += $$IN_PWD/generators_global.h

    headersG3000.path = /usr/local/include/radiy/generators/G3000
    headersG3000.files += $$IN_PWD/G3000/commands.h \
                          $$IN_PWD/G3000/g3000.h

    INSTALLS += headersGeneral \
                headersG3000
}

win32 {
    target.path = $$IN_PWD/tmp
    INSTALLS = target

 #   headersGeneral.path = "C:/Program Files/Radiy/include/generators"
 #   headersGeneral.files += $$IN_PWD/generators_global.h

 #   headersG3000.path = "C:/Program Files/Radiy/include/generators/G3000"
  #  headersG3000.files += $$IN_PWD/G3000/commands.h \
#                          $$IN_PWD/G3000/g3000.h

 #   INSTALLS += headersGeneral \
 #               headersG3000
}

DISTFILES += \
    calibration.txt \
    calibration6009.txt \
    calibration6009.txt

RESOURCES += \
    amp.qrc
