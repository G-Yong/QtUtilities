QT       += core
QT       -= gui

TARGET    = dump_example
CONFIG   += c++17 console
CONFIG   -= app_bundle
msvc: QMAKE_CXXFLAGS += /utf-8
else: QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SOURCES  += main.cpp
