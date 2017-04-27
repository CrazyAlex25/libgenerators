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

SOURCES += generators.cpp \
        G3000/g3000.cpp

HEADERS += generators.h\
        generators_global.h \
    G3000/commands.h \
    G3000/g3000.h

VERSION = 1.0.4

unix {
    target.path = /usr/lib/radiy
    INSTALLS += target

    headersGeneral.path = /usr/include/radiy/generators
    headersGeneral.files += $$IN_PWD/generators_global.h

    headersG3000.path = /usr/include/radiy/generators/G3000
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
