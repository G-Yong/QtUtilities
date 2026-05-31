QT       += core
QT       -= gui

TARGET    = system_example
CONFIG   += c++17 console
CONFIG   -= app_bundle
QMAKE_CXXFLAGS += /utf-8

SOURCES  += main.cpp

# On MinGW you must link psapi explicitly (MSVC links it automatically via
# the #pragma comment in system.h).
win32-g++: LIBS += -lpsapi
