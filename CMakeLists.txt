# Copyright (C) 2022, 2023, 2024
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

# Required: Qt6
find_package(Qt6 6.6.0 REQUIRED COMPONENTS OpenGLWidgets Multimedia LinguistTools)
# Optional: QVR for Virtual Reality support
find_package(QVR 4.1.0 QUIET)
if(QVR_FOUND)
    add_definitions(-DWITH_QVR)
    include_directories(${QVR_INCLUDE_DIRS})
    link_directories(${QVR_LIBRARY_DIRS})
endif()

# The executable
add_executable(bino
	src/main.cpp src/version.hpp
	src/log.hpp src/log.cpp
	src/tools.hpp src/tools.cpp
	src/screen.hpp src/screen.cpp src/tiny_obj_loader.h
	src/modes.hpp src/modes.cpp
	src/metadata.hpp src/metadata.cpp
	src/playlist.hpp src/playlist.cpp
	src/videoframe.hpp src/videoframe.cpp
	src/videosink.hpp src/videosink.cpp
	src/bino.hpp src/bino.cpp
	src/qvrapp.hpp src/qvrapp.cpp
	src/widget.hpp src/widget.cpp
	src/commandinterpreter.hpp src/commandinterpreter.cpp
	src/playlisteditor.hpp src/playlisteditor.cpp
	src/gui.hpp src/gui.cpp
	src/appicon.rc)
qt6_add_translations(bino TS_FILES i18n/bino_de.ts i18n/bino_ka.ts i18n/bino_zh.ts)
qt6_add_resources(bino "misc" PREFIX "/" FILES
	src/shader-color.vert.glsl
	src/shader-color.frag.glsl
	src/shader-view.vert.glsl
	src/shader-view.frag.glsl
	src/shader-display.vert.glsl
	src/shader-display.frag.glsl
	src/shader-vrdevice.vert.glsl
	src/shader-vrdevice.frag.glsl
	res/bino-logo-small.svg res/bino-logo-small-512.png)
set_target_properties(bino PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(bino PRIVATE Qt6::OpenGLWidgets Qt6::Multimedia ${QVR_LIBRARIES})
install(TARGETS bino RUNTIME DESTINATION bin)

# The manual and man page (optional, only if pandoc is found)
find_program(PANDOC NAMES pandoc DOC "pandoc executable")
if(PANDOC)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/bino-manual.html"
	COMMAND ${PANDOC} "-s" "-t" "html" "--toc" "--css" "bino-manual.css" "${CMAKE_SOURCE_DIR}/doc/bino-manual.md" "-o" "${CMAKE_BINARY_DIR}/bino-manual.html"
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	DEPENDS "${CMAKE_SOURCE_DIR}/doc/bino-manual.md"
	COMMENT "Generating HTML manual with pandoc" VERBATIM)
    add_custom_target(manual ALL DEPENDS "${CMAKE_BINARY_DIR}/bino-manual.html")
    install(FILES "doc/bino-manual.css" "${CMAKE_BINARY_DIR}/bino-manual.html" DESTINATION share/doc/bino)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/bino.1"
	COMMAND ${PANDOC} "-s" "-t" "man" "${CMAKE_SOURCE_DIR}/doc/bino-manual.md" "-o" "${CMAKE_BINARY_DIR}/bino.1"
	WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
	DEPENDS "${CMAKE_SOURCE_DIR}/doc/bino-manual.md"
	COMMENT "Generating man page with pandoc" VERBATIM)
    add_custom_target(manpage ALL DEPENDS "${CMAKE_BINARY_DIR}/bino.1")
    install(FILES "${CMAKE_BINARY_DIR}/bino.1" DESTINATION share/man/man1)
endif()

# Add auxiliary files for Linux-ish systems
if(UNIX)
    install(FILES "res/bino-logo-small-16.png"  RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/16x16/apps)
    install(FILES "res/bino-logo-small-22.png"  RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/22x22/apps)
    install(FILES "res/bino-logo-small-32.png"  RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/32x32/apps)
    install(FILES "res/bino-logo-small-48.png"  RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/48x48/apps)
    install(FILES "res/bino-logo-small-64.png"  RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/64x64/apps)
    install(FILES "res/bino-logo-small-128.png" RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/128x128/apps)
    install(FILES "res/bino-logo-small-256.png" RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/256x256/apps)
    install(FILES "res/bino-logo-small-512.png" RENAME "org.bino3d.bino.png" DESTINATION share/icons/hicolor/512x512/apps)
    install(FILES "res/bino-logo-small.svg"     RENAME "org.bino3d.bino.svg" DESTINATION share/icons/hicolor/scalable/apps)
    install(FILES "res/org.bino3d.bino.desktop" DESTINATION share/applications)
    install(FILES "res/org.bino3d.bino.metainfo.xml" DESTINATION share/metainfo)
endif()

if (QVR_FOUND)
    message(STATUS "Build Bino with QVR support: YES")
else()
    message(STATUS "Build Bino with QVR support: NO")
endif()
if (PANDOC)
    message(STATUS "Build manual and man page with pandoc: YES")
else()
    message(STATUS "Build manual and man page with pandoc: NO")
endif()
