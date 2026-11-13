QT += core gui quick quickcontrols2 testlib dbus concurrent
CONFIG += c++17 testcase
TARGET = sizer_tests
TEMPLATE = app

INCLUDEPATH += ../src

HEADERS += ../src/backend.h ../src/filepicker.h ../src/imagelist.h ../src/resizer.h \
    ../src/theme.h ../src/translator.h
SOURCES += sizer_tests.cpp ../src/backend.cpp ../src/imagelist.cpp ../src/resizer.cpp \
    ../src/theme.cpp ../src/translator.cpp
