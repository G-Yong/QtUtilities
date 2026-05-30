QT       += core gui

TARGET    = cv_example
CONFIG   += c++17 console
CONFIG   -= app_bundle
QMAKE_CXXFLAGS += /utf-8

SOURCES  += main.cpp

INCLUDEPATH += D:/Qt/opencv4.1.0/include

CONFIG(debug, debug|release) {
    LIBS += -LD:/Qt/opencv4.1.0/x64/vc15/lib -lopencv_world410d
} else {
    LIBS += -LD:/Qt/opencv4.1.0/x64/vc15/lib -lopencv_world410
}
