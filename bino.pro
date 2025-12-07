# This qmake .pro file is only for building for the WASM platform,
# use CMake instead.

HEADERS = src/version.hpp src/tiny_obj_loader.h src/log.hpp src/tools.hpp src/screen.hpp src/modes.hpp src/metadata.hpp src/playlist.hpp src/videoframe.hpp src/videosink.hpp src/bino.hpp src/qvrapp.hpp src/widget.hpp src/commandinterpreter.hpp src/playlisteditor.hpp src/gui.hpp src/urlloader.hpp src/digestiblemedia.hpp

SOURCES = src/main.cpp src/log.cpp src/tools.cpp src/screen.cpp src/modes.cpp src/metadata.cpp src/playlist.cpp src/videoframe.cpp src/videosink.cpp src/bino.cpp src/qvrapp.cpp src/widget.cpp src/commandinterpreter.cpp src/playlisteditor.cpp src/gui.cpp src/urlloader.cpp src/digestiblemedia.cpp

RC_FILE = src/appicon.rc

resources.files = res/bino-logo-small-512.png src/shader-color.vert.glsl src/shader-color.frag.glsl src/shader-view.vert.glsl src/shader-view.frag.glsl src/shader-display.vert.glsl src/shader-display.frag.glsl src/shader-vrdevice.vert.glsl src/shader-vrdevice.frag.glsl
resources.prefix = /

RESOURCES = resources

CONFIG += release

QMAKE_CXXFLAGS += -Oz -flto
QMAKE_LFLAGS += -sASYNCIFY -sFULL_ES3=1 -Oz -flto

QT += openglwidgets multimedia svg
