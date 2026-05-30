QT       += core
QT       -= gui

TARGET    = log_example
CONFIG   += c++17 console
CONFIG   -= app_bundle
QMAKE_CXXFLAGS += /utf-8

DEFINES  += QT_MESSAGELOGCONTEXT

SOURCES  += main.cpp
