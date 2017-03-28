#-------------------------------------------------
#
# Project created by QtCreator 2017-03-27T15:48:48
#
#-------------------------------------------------

QT       += network

QT       -= gui

TARGET = libradiy-devel
TEMPLATE = lib

DEFINES += LIBRADIYDEVEL_LIBRARY

SOURCES += libradiydevel.cpp \
    generators/G3000/g3000.cpp

HEADERS += libradiydevel.h\
        libradiy-devel_global.h \
    generators/G3000/g3000.h \
    generators/G3000/commands.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}
