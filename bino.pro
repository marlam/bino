# Use CMake to build bino.
# This qmake project file is only for special cases where CMake does not work,
# and it does not support building with QVR.

HEADERS = version.hpp \
        log.hpp \
        tools.hpp \
        screen.hpp \
	tiny_obj_loader.h \
        metadata.hpp \
	playlist.hpp \
        videoframe.hpp \
        videosink.hpp \
        bino.hpp \
        qvrapp.hpp \
	widget.hpp \
	mainwindow.hpp

SOURCES = main.cpp \
	log.cpp \
	tools.cpp \
	screen.cpp \
	metadata.cpp \
	playlist.cpp \
	videoframe.cpp \
	videosink.cpp \
	bino.cpp \
	qvrapp.cpp \
	widget.cpp \
	mainwindow.cpp

RESOURCES = resources.qrc

TRANSLATIONS = bino_de.ts

RC_FILE = appicon.rc

CONFIG += release

QT += openglwidgets multimedia

QMAKE_CXXFLAGS += -std=c++17 -fopenmp
