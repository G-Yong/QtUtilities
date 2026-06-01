QT       += core
QT       -= gui

TARGET    = system_example
CONFIG   += c++17 console
CONFIG   -= app_bundle
msvc: QMAKE_CXXFLAGS += /utf-8
else: QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SOURCES  += main.cpp

# On MinGW you must link psapi explicitly (MSVC links it automatically via
# the #pragma comment in system.h).
win32-g++: LIBS += -lpsapi
