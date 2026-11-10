QT += core gui qml quick quickcontrols2

unix:!macx {
    QT += dbus
    LIBS += -lLayerShellQtInterface
}

win32 {
    LIBS += -luser32 -lshell32
    RC_FILE = pkgbuild/blackout.rc
}

CONFIG += c++17 release
TARGET = blackout
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
