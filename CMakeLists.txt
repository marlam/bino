# Copyright (C) 2022
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

cmake_minimum_required(VERSION 3.12)
set(CMAKE_FIND_PACKAGE_SORT_ORDER NATURAL)
set(CMAKE_FIND_PACKAGE_SORT_DIRECTION DEC)
set(CMAKE_AUTOMOC ON)

project(Bino LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)

find_package(Qt6 6.3.0 REQUIRED COMPONENTS OpenGLWidgets Multimedia LinguistTools)
find_package(QVR 4.0.0)
if(QVR_FOUND)
    add_definitions(-DWITH_QVR)
    include_directories(${QVR_INCLUDE_DIRS})
    link_directories(${QVR_LIBRARY_DIRS})
endif()

qt6_add_resources(RESOURCES resources.qrc)
if(WIN32)
    set(APPICON_RESOURCE appicon.rc)
endif()

add_executable(bino
	main.cpp version.hpp
        log.hpp log.cpp
        tools.hpp tools.cpp
        screen.hpp screen.cpp tiny_obj_loader.h
        metadata.hpp metadata.cpp
	playlist.hpp playlist.cpp
        videoframe.hpp videoframe.cpp
        videosink.hpp videosink.cpp
        bino.hpp bino.cpp
        qvrapp.hpp qvrapp.cpp
	widget.hpp widget.cpp
	commandinterpreter.hpp commandinterpreter.cpp
	mainwindow.hpp mainwindow.cpp
	${RESOURCES} ${APPICON_RESOURCE})
qt6_add_translations(bino TS_FILES bino_de.ts)
set_target_properties(bino PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(bino PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia ${QVR_LIBRARIES})
install(TARGETS bino RUNTIME DESTINATION bin)
