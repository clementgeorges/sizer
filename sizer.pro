QT += core gui qml quick quickcontrols2 dbus concurrent

CONFIG += c++17 release
TARGET = sizer
TEMPLATE = app

HEADERS += \
    src/backend.h \
    src/cli.h \
    src/filepicker.h \
    src/imagelist.h \
    src/portalfilepicker.h \
    src/resizer.h \
    src/theme.h \
    src/translator.h

SOURCES += \
    src/main.cpp \
    src/backend.cpp \
    src/cli.cpp \
    src/imagelist.cpp \
    src/portalfilepicker.cpp \
    src/resizer.cpp \
    src/theme.cpp \
    src/translator.cpp

RESOURCES += src/resources.qrc
