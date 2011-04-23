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
        subtitle_renderer.h subtitle_renderer.cpp \
	audio_output.h audio_output.cpp \
	player.h player.cpp \
	player_qt.h player_qt.cpp \
	qt_app.h qt_app.cpp \
	lib_versions.h lib_versions.cpp \
	main.cpp

ICONS = \
	icons/application-exit.png \
	icons/applications-internet.png \
	icons/document-open.png \
	icons/help-about.png \
	icons/help-contents.png \
	icons/media-playback-pause.png \
	icons/media-playback-start.png \
	icons/media-playback-stop.png \
	icons/media-seek-backward.png \
	icons/media-seek-forward.png \
	icons/preferences-desktop-keyboard.png \
	icons/view-fullscreen.png \
	icons/view-restore.png

EXTRA_DIST = \
	video_output_color.fs.glsl \
	video_output_render.fs.glsl \
	logo/README \
	logo/bino_logo.ico \
	logo/bino_logo.png \
	logo/bino_logo.svg \
	logo/bino_logo_small.svg \
	logo/bino_logo_small_16x16.png \
	logo/bino_logo_small_32x32.png \
	logo/bino_logo_small_48x48.png \
	logo/bino_logo_small_64x64.png \
	icons/COPYING \
	icons/README \
	$(ICONS)

qt_resources.qrc:
	$(AM_V_GEN)(echo '<!DOCTYPE RCC><RCC version="1.0">'; \
	echo '<qresource>'; \
	echo '<file alias="AUTHORS">$(top_srcdir)/AUTHORS</file>'; \
	echo '<file alias="LICENSE">$(top_srcdir)/COPYING</file>'; \
	echo '<file alias="logo/bino_logo.png">$(top_srcdir)/src/logo/bino_logo.png</file>'; \
	echo '<file alias="logo/bino_logo_small_64x64.png">$(top_srcdir)/src/logo/bino_logo_small_64x64.png</file>'; \
	for icon in $(ICONS); do echo '<file alias="'"$${icon}"'">$(top_srcdir)/src/'"$${icon}"'</file>'; done; \
	echo '</qresource>'; \
	echo '</RCC>') > $@

qt_resources-rcc.cpp: $(filter %.png,$(EXTRA_DIST)) ../AUTHORS ../COPYING

nodist_bino_SOURCES = \
	qt_resources.qrc \
	qt_resources-rcc.cpp \
	player_qt-moc.cpp \
	video_output_qt-moc.cpp \
	video_output_color.fs.glsl.h \
	video_output_render.fs.glsl.h

BUILT_SOURCES = $(nodist_bino_SOURCES)

CLEANFILES = $(BUILT_SOURCES)

DEFS += -DDOCDIR=\"$(docdir)\" -DLOCALEDIR=\"$(localedir)\"

AM_CPPFLAGS = -I$(top_srcdir)/src/base \
	$(libavformat_CFLAGS) $(libswscale_CFLAGS) $(libass_CFLAGS) \
	$(libopenal_CFLAGS) $(libglew_CFLAGS) $(libqtopengl_CFLAGS)

bino_LDADD = $(top_builddir)/src/base/libbase.a $(LIBINTL) \
	$(libavformat_LIBS) $(libswscale_LIBS) $(libass_LIBS) \
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
