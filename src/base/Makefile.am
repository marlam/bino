# Copyright (C) 2010-2011
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

noinst_LIBRARIES = libbase.a
libbase_a_SOURCES = \
	str.h str.cpp \
	msg.h msg.cpp \
	debug.h debug.cpp \
	exc.h exc.cpp \
	intcheck.h \
	opt.h opt.cpp \
	timer.h timer.cpp \
	blob.h

if HAVE_LIBEQUALIZER
libbase_a_SOURCES += s11n.h s11n.cpp
endif
