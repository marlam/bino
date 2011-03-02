# Copyright (C) 2010-2011
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

SUBDIRS = base

bin_PROGRAMS = bino

bino_SOURCES = \
	media_data.h media_data.cpp \
	media_object.h media_object.cpp \
	media_input.h media_input.cpp \
	controller.h controller.cpp \
        video_output.h video_output.cpp \
        video_output_qt.h video_output_qt.cpp \
	xgl.h xgl.cpp \
	audio_output.h audio_output.cpp \
	player.h player.cpp \
	player_qt.h player_qt.cpp \
	qt_app.h qt_app.cpp \
	lib_versions.h lib_versions.cpp \
	main.cpp

EXTRA_DIST = \
	qt_resources.qrc \
	logo/README \
	logo/bino_logo.ico \
	logo/bino_logo.png \
	logo/bino_logo.svg \
	logo/bino_logo_small.svg \
	logo/bino_logo_small_16x16.png \
	logo/bino_logo_small_32x32.png \
	logo/bino_logo_small_48x48.png \
	logo/bino_logo_small_64x64.png \
	icons/README \
	icons/play.ipe icons/play.png \
	icons/pause.ipe icons/pause.png \
	icons/stop.ipe icons/stop.png \
	icons/fullscreen.ipe icons/fullscreen.png \
	icons/center.ipe icons/center.png \
	icons/f.ipe icons/f.png \
	icons/ff.ipe icons/ff.png \
	icons/fff.ipe icons/fff.png \
	icons/b.ipe icons/b.png \
	icons/bb.ipe icons/bb.png \
	icons/bbb.ipe icons/bbb.png \
	video_output_color.fs.glsl \
	video_output_render.fs.glsl

qt_resources-rcc.cpp: $(filter %.png,$(EXTRA_DIST)) ../AUTHORS ../COPYING

nodist_bino_SOURCES = \
	qt_resources-rcc.cpp \
	player_qt-moc.cpp \
	video_output_qt-moc.cpp \
	video_output_color.fs.glsl.h \
	video_output_render.fs.glsl.h

BUILT_SOURCES = $(nodist_bino_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

DEFS += -DDOCDIR=\"$(docdir)\"

AM_CPPFLAGS = -I$(top_srcdir)/src/base \
	$(libavformat_CFLAGS) $(libswscale_CFLAGS) \
	$(libopenal_CFLAGS) $(libglew_CFLAGS) $(libqtopengl_CFLAGS)

bino_LDADD = $(top_builddir)/src/base/libbase.a \
	$(libavformat_LIBS) $(libswscale_LIBS) \
	$(libopenal_LIBS) $(libglew_LIBS) $(libqtopengl_LIBS)

if HAVE_LIBEQUALIZER
bino_SOURCES += player_equalizer.h player_equalizer.cpp
AM_CPPFLAGS += $(libEqualizer_CFLAGS) -DGLEW_MX
bino_LDADD += $(libEqualizer_LIBS)
else
AM_CPPFLAGS += $(libglew_CFLAGS)
bino_LDADD += $(libglew_LIBS)
endif

if W32
bino_SOURCES += logo/bino_logo.ico
.ico.o:
	$(AM_V_GEN)echo "IDI_ICON1 ICON DISCARDABLE \"$<\"" | $(WINDRES) -o $@
endif

moc_verbose = $(moc_verbose_$(V))
moc_verbose_ = $(moc_verbose_$(AM_DEFAULT_VERBOSITY))
moc_verbose_0 = @echo "  MOC   " $@;

%-moc.cpp: %.h
	$(moc_verbose)$(MOC) -o $@ $<

rcc_verbose = $(rcc_verbose_$(V))
rcc_verbose_ = $(rcc_verbose_$(AM_DEFAULT_VERBOSITY))
rcc_verbose_0 = @echo "  RCC   " $@;

%-rcc.cpp: %.qrc
	$(rcc_verbose)$(RCC) -o $@ $<

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
