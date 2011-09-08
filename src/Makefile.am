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
	lib_versions.h lib_versions.cpp \
	main.cpp

ICONS_LOCAL_IPE = \
	icons-local/input-layout-mono.ipe \
	icons-local/input-layout-separate-left-right.ipe \
	icons-local/input-layout-separate-right-left.ipe \
	icons-local/input-layout-top-bottom.ipe \
	icons-local/input-layout-top-bottom-half.ipe \
	icons-local/input-layout-bottom-top.ipe \
	icons-local/input-layout-bottom-top-half.ipe \
	icons-local/input-layout-left-right.ipe \
	icons-local/input-layout-left-right-half.ipe \
	icons-local/input-layout-right-left.ipe \
	icons-local/input-layout-right-left-half.ipe \
	icons-local/input-layout-even-odd-rows.ipe \
	icons-local/input-layout-odd-even-rows.ipe \
	icons-local/output-type-mono-left.ipe \
	icons-local/output-type-mono-right.ipe \
	icons-local/output-type-top-bottom.ipe \
	icons-local/output-type-top-bottom-half.ipe \
	icons-local/output-type-left-right.ipe \
	icons-local/output-type-left-right-half.ipe \
	icons-local/output-type-even-odd-rows.ipe \
	icons-local/output-type-even-odd-columns.ipe \
	icons-local/output-type-checkerboard.ipe \
	icons-local/output-type-hdmi-frame-pack.ipe \
	icons-local/output-type-red-cyan.ipe \
	icons-local/output-type-green-magenta.ipe \
	icons-local/output-type-amber-blue.ipe \
	icons-local/output-type-red-green.ipe \
	icons-local/output-type-red-blue.ipe \
	icons-local/output-type-stereo.ipe

ICONS = \
	icons/application-exit.png \
	icons/applications-internet.png \
        icons/camera-web.png \
	icons/document-open.png \
	icons/help-about.png \
	icons/help-contents.png \
	icons/media-playback-pause.png \
	icons/media-playback-start.png \
	icons/media-playback-stop.png \
	icons/media-playlist-repeat.png \
	icons/media-seek-backward.png \
	icons/media-seek-forward.png \
	icons/preferences-desktop-keyboard.png \
	icons/view-fullscreen.png \
	icons/view-restore.png \
	$(patsubst %.ipe,%.png,$(ICONS_LOCAL_IPE))

EXTRA_DIST = \
	video_output_color.fs.glsl \
	video_output_render.fs.glsl \
	logo/README \
	logo/bino_logo.ico \
	logo/bino_logo.png \
	logo/bino_logo.svg \
	icons/COPYING \
	icons/README \
	icons-local/README \
	$(ICONS) $(ICONS_LOCAL_IPE)

qt_resources.qrc:
	$(AM_V_GEN)(echo '<!DOCTYPE RCC><RCC version="1.0">'; \
	echo '<qresource>'; \
	echo '<file alias="AUTHORS">$(top_srcdir)/AUTHORS</file>'; \
	echo '<file alias="LICENSE">$(top_srcdir)/COPYING</file>'; \
	echo '<file alias="logo/bino_logo.png">$(top_srcdir)/src/logo/bino_logo.png</file>'; \
	echo '<file alias="logo/64x64/bino.png">$(top_srcdir)/src/logo/64x64/bino.png</file>'; \
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
	$(libavformat_CFLAGS) $(libavdevice_CFLAGS) $(libswscale_CFLAGS) $(libass_CFLAGS) \
	$(libopenal_CFLAGS) $(libgl_CFLAGS) $(libglew_CFLAGS) $(libqtopengl_CFLAGS)

bino_LDADD = $(top_builddir)/src/base/libbase.a $(LIBINTL) $(LIBICONV) \
	$(libavformat_LIBS) $(libavdevice_LIBS) $(libswscale_LIBS) $(libass_LIBS) \
	$(libopenal_LIBS) $(libgl_LIBS) $(libglew_LIBS) $(libqtopengl_LIBS)

if HAVE_LIBEQUALIZER
bino_SOURCES += player_equalizer.h player_equalizer.cpp
AM_CPPFLAGS += $(libEqualizer_CFLAGS) -DGLEW_MX
bino_LDADD += $(libEqualizer_LIBS)
endif

if HAVE_LIBLIRCCLIENT
bino_SOURCES += lirc.h lirc.cpp
AM_CPPFLAGS += $(liblircclient_CFLAGS)
bino_LDADD += $(liblircclient_LIBS)
endif

if W32
bino_SOURCES += logo/bino_logo.ico
.ico.o:
	$(AM_V_GEN)echo "IDI_ICON1 ICON DISCARDABLE \"$<\"" | $(WINDRES) -o $@
endif

iconsdatadir = $(datadir)/icons/hicolor
icons16dir = $(iconsdatadir)/16x16/apps
icons32dir = $(iconsdatadir)/32x32/apps
icons48dir = $(iconsdatadir)/48x48/apps
icons64dir = $(iconsdatadir)/64x64/apps
iconsscaldir = $(iconsdatadir)/scalable/apps
dist_icons16_DATA = logo/16x16/bino.png
dist_icons32_DATA = logo/32x32/bino.png
dist_icons48_DATA = logo/48x48/bino.png
dist_icons64_DATA = logo/64x64/bino.png
dist_iconsscal_DATA = logo/scalable/bino.svg
update-icon-cache:
	test -n "$(DESTDIR)" -o -z "$(GTK_UPDATE_ICON_CACHE)" || (umask 022 && $(GTK_UPDATE_ICON_CACHE) -q -f -t "$(iconsdatadir)")

appdatadir = $(datadir)/applications
dist_appdata_DATA = bino.desktop
update-desktop-database:
	test -n "$(DESTDIR)" -o -z "$(UPDATE_DESKTOP_DATABASE)" || (umask 022 && $(UPDATE_DESKTOP_DATABASE) "$(appdatadir)")

install-data-hook: update-icon-cache update-desktop-database
uninstall-hook: update-icon-cache update-desktop-database


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
