# Copyright (C) 2010, 2011, 2012
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

man_MANS = bino.1

info_TEXINFOS = bino.texi

ICONS_LOCAL = \
	input-layout-mono.png \
	input-layout-separate-left-right.png \
	input-layout-separate-right-left.png \
	input-layout-alternating-left-right.png \
	input-layout-alternating-right-left.png \
	input-layout-top-bottom.png \
	input-layout-top-bottom-half.png \
	input-layout-bottom-top.png \
	input-layout-bottom-top-half.png \
	input-layout-left-right.png \
	input-layout-left-right-half.png \
	input-layout-right-left.png \
	input-layout-right-left-half.png \
	input-layout-even-odd-rows.png \
	input-layout-odd-even-rows.png \
	output-type-stereo.png \
	output-type-alternating.png \
	output-type-mono-left.png \
	output-type-mono-right.png \
	output-type-top-bottom.png \
	output-type-top-bottom-half.png \
	output-type-left-right.png \
	output-type-left-right-half.png \
	output-type-even-odd-rows.png \
	output-type-even-odd-columns.png \
	output-type-checkerboard.png \
	output-type-hdmi-frame-pack.png \
	output-type-red-cyan.png \
	output-type-green-magenta.png \
	output-type-amber-blue.png \
	output-type-red-green.png \
	output-type-red-blue.png

input-layout-%.png:
	$(LN_S) $(top_srcdir)/src/icons-local/$@

output-type-%.png:
	$(LN_S) $(top_srcdir)/src/icons-local/$@

IMAGES = \
	multi-display-vrlab.jpg \
	multi-display-rotated.jpg \
	gamma-pattern-tb.png \
	crosstalk-pattern-tb.png \
	$(ICONS_LOCAL)

bino_TEXINFOS = bino.texi $(IMAGES)

AM_MAKEINFOHTMLFLAGS = --no-split

dist_html_DATA = bino.html $(IMAGES) audio-video-sync.mp4

EXTRA_DIST = $(man_MANS)
