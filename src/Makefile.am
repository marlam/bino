# Copyright (C) 2010  Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

SUBDIRS = base

bin_PROGRAMS = bino

bino_SOURCES = \
	decoder.h decoder.cpp \
	decoder_ffmpeg.h decoder_ffmpeg.cpp \
        input.h input.cpp \
	controller.h controller.cpp \
        video_output.h video_output.cpp \
	xgl.h xgl.cpp \
        video_output_opengl.h video_output_opengl.cpp \
	audio_output.h audio_output.cpp \
	audio_output_openal.h audio_output_openal.cpp \
	player.h player.cpp \
	main.cpp

EXTRA_DIST = \
	video_output_opengl.fs.glsl

nodist_bino_SOURCES = \
	video_output_opengl.fs.glsl.h

BUILT_SOURCES = $(nodist_bino_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

AM_CPPFLAGS = -I$(top_srcdir)/src/base \
	$(libavformat_CFLAGS) $(libswscale_CFLAGS) \
	$(libglut_CFLAGS) $(libglew_CFLAGS) \
	$(libalut_CFLAGS)

bino_LDADD = $(top_builddir)/src/base/libbase.a \
	$(libavformat_LIBS) $(libswscale_LIBS) \
	$(libglut_LIBS) $(libglew_LIBS) \
	$(libalut_LIBS)

%.glsl.h: %.glsl
	$(AM_V_GEN)MACRONAME="`echo $< | sed -e s/^.*\\\/// -e s/\\\.glsl$$// -e s/[\\\.-]/_/g | tr [a-z] [A-Z]`_GLSL_STR"; \
	(echo "/* GENERATED AUTOMATICALLY FROM $< */"; \
	 echo "#ifndef $$MACRONAME"; \
	 echo "#define $$MACRONAME \\"; \
	 sed -e s/\\\\/\\\\\\\\/g \
	     -e s/\"/\\\\\"/g \
	     -e s/^/\"/ \
	     -e s/$$/\\\\n\"\\\\/ < $<; \
	 echo \"\"; \
	 echo "#endif") > $@
