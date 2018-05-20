# required: apt-get install qt5-default

QT_VERSION = 5

QT       += core widgets

TARGET = QtTree
TEMPLATE = app


SOURCES += main.cpp\
        db_reader.cpp\
        mainwindow.cpp \
    tree_widget_item.cpp \
    item_delegate.cpp \
    find_dialog.cpp

HEADERS  += mainwindow.h \
    db_reader.h \
    memory_pool.h \
    db_reader_thread.h \
    tree_widget_item.h \
    item_delegate.h \
    find_dialog.h

INCLUDEPATH += $$PWD/../3dparty/psc/include

DESTDIR = $$PWD

LIBS += -L$$PWD/../3dparty/psc -lpsc

DEFINES += _LINUX
#DEFINES += DEBUG

#QTPLUGIN += qsqloci qgif
#CONFIG += static

QMAKE_CXXFLAGS += -std=c++11 -ggdb -g3 -O2 -fno-omit-frame-pointer
#QMAKE_CFLAGS += -fsanitize=address,undefined -fno-omit-frame-pointer -O2 -g3 -ggdb
#QMAKE_LFLAGS += -fsanitize=address,undefined
#QMAKE_CXXFLAGS += -D_GLIBCXX_DEBUG -D_GLIBCXX_DEBUG_PEDANTIC

FORMS    += mainwindow.ui
