QT += core gui quick quickcontrols2 testlib

unix:!macx {
    QT += dbus
}

CONFIG += c++17 testcase
TARGET = blackout_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/theme.h ../src/translator.h
SOURCES += blackout_tests.cpp ../src/backend.cpp ../src/theme.cpp ../src/translator.cpp
