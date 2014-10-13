dnl Process this file with autoconf to produce a configure script.

dnl configure.ac
dnl
dnl This file is part of bino, a 3D video player.
dnl
dnl Copyright (C) 2010, 2011, 2012, 2013, 2014
dnl Martin Lambers <marlam@marlam.de>
dnl Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
dnl Binocle <http://binocle.com> (author: Olivier Letz <oletz@binocle.com>)
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Autotools init stuff
AC_PREREQ([2.63])
AC_INIT([Bino], [1.6.1], [bino-list@nongnu.org], [bino], [http://bino3d.org/])
AC_CONFIG_SRCDIR([src/main.cpp])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_TARGET
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AM_INIT_AUTOMAKE([1.11.1 color-tests silent-rules tar-pax no-texinfo.tex -Wall -Wno-portability])
AM_SILENT_RULES([yes])
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_RANLIB
AC_PROG_LN_S
AC_LANG([C++])
AC_C_BIGENDIAN

dnl Check if the C++ compiler works. Autoconf will not do that for us!?
AC_MSG_CHECKING([whether the C++ compiler works])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])], [COMPILER_WORKS=1], [COMPILER_WORKS=0])
if test "$COMPILER_WORKS" = "1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([The C++ compiler was not found or does not work.])
fi

dnl Useful definitions
AC_DEFINE_UNQUOTED([PLATFORM], ["${target}"], [Platform triplet])

dnl Special handling of W32
case "${target}" in *-*-mingw32*) w32="yes" ;; *) w32="no " ;; esac
if test "$w32" = "yes"; then
    CXXFLAGS="-mthreads $CXXFLAGS"
    LDFLAGS="-mthreads -mwindows $LDFLAGS"
    AC_ARG_VAR([WINDRES], [windres command])
    if test -z "$WINDRES"; then
        AC_CHECK_TOOLS([WINDRES], [windres])
    fi
    if test -z "$WINDRES"; then
        AC_MSG_ERROR([windres not found])
    fi
    AC_ARG_VAR([MAKENSIS], [makensis command])
    if test -z "$MAKENSIS"; then
        AC_CHECK_TOOLS([MAKENSIS], [makensis])
    fi
    if test -z "$MAKENSIS"; then
        AC_MSG_WARN([optional tool makensis not found])
    fi
else
    MAKENSIS=""
fi
AM_CONDITIONAL([HAVE_MAKENSIS], [test -n "$MAKENSIS"])
AM_CONDITIONAL([W32], [test "$w32" = "yes"])

dnl Configure switches
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug], [Enable debugging checks. Disabled by default.])],
    [if test "$enableval" = "yes"; then ndebug=0; else ndebug=1; fi], [ndebug=1])
if test "$ndebug" = "1"; then
    AC_DEFINE_UNQUOTED([NDEBUG], [1], [Define to 1 to disable assertions and other debugging checks.])
fi

dnl Feature checks needed by the base modules
AC_LANG_PUSH([C])
dnl - dbg
AC_CHECK_FUNCS([backtrace sigaction])
dnl - endianness
dnl AC_C_BIGENDIAN
dnl - fio
dnl case "${target}" in *-*-mingw*) LIBS="$LIBS -lshlwapi" ;; esac
dnl AC_CHECK_FUNCS([fdatasync fnmatch fseeko ftello getpwuid link mmap posix_fadvise readdir_r symlink])
dnl - opt
case "${target}" in *-*-mingw*) CPPFLAGS="$CPPFLAGS -D_BSD_SOURCE" ;; esac
AC_CHECK_DECLS([optreset], [], [], [#include <getopt.h>])
dnl - str
AM_ICONV([])
AC_CHECK_FUNCS([nl_langinfo vasprintf wcswidth])
dnl - sys
dnl AC_CHECK_FUNCS([nanosleep sysconf sched_yield])
dnl - thread
AC_MSG_CHECKING([for GCC atomic builtins])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]],
    [[int b, x = 0; b = __sync_bool_compare_and_swap(&x, 0, 1);]])],
    HAVE_GCC_ATOMIC_BUILTINS=1, HAVE_GCC_ATOMIC_BUILTINS=0)
if test "$HAVE_GCC_ATOMIC_BUILTINS" = "1"; then
    AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
    AC_MSG_ERROR([GCC atomic builtins not found. Please use a newer compiler.])
fi
AC_SEARCH_LIBS([pthread_create], [pthread])
CPPFLAGS_bak="$CPPFLAGS"
LDFLAGS_bak="$LDFLAGS"
CPPFLAGS="-pthread $CPPFLAGS"
LDFLAGS="-pthread $LDFLAGS"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])], [], [CPPFLAGS="$CPPFLAGS_bak"; LDFLAGS="$LDFLAGS_bak"])
dnl - timer
AC_SEARCH_LIBS([clock_gettime], [rt])
AC_CHECK_FUNCS([clock_gettime])
AC_LANG_POP([C])

dnl Gettext
AC_LANG_PUSH([C])
AM_ICONV([])
if test "$am_cv_func_iconv" != "yes"; then
    AC_MSG_WARN([required library libiconv not found])
fi
AM_GNU_GETTEXT([external])
AC_LANG_POP([C])

dnl pkg-config (required to detect libraries)
PKG_PROG_PKG_CONFIG([])
if test -z "$PKG_CONFIG"; then
    AC_MSG_WARN([required tool pkg-config not found])
    AC_MSG_WARN([required libraries cannot be detected])
    AC_MSG_WARN([please install pkg-config; Debian package: pkg-config])
fi

dnl FFmpeg
PKG_CHECK_MODULES([libavformat], [libavformat >= 52.110.0 libavcodec libavutil], [HAVE_LIBAVFORMAT=1], [HAVE_LIBAVFORMAT=0])
if test "$HAVE_LIBAVFORMAT" != "1"; then
    AC_MSG_WARN([required library libavformat >= 52.110.0 not found:])
    AC_MSG_WARN([$libavformat_PKG_ERRORS])
    AC_MSG_WARN([libavformat >= 52.110.0 is provided by libav >= 0.7 or FFmpeg >= 0.7])
fi
PKG_CHECK_MODULES([libavdevice], [libavdevice >= 52.5.0], [HAVE_LIBAVDEVICE=1], [HAVE_LIBAVDEVICE=0])
if test "$HAVE_LIBAVDEVICE" != "1"; then
    AC_MSG_WARN([required library libavdevice >= 52.5.0 not found:])
    AC_MSG_WARN([$libavdevice_PKG_ERRORS])
    AC_MSG_WARN([libavdevice >= 52.5.0 is provided by libav >= 0.7 or FFmpeg >= 0.7])
fi
PKG_CHECK_MODULES([libswscale], [libswscale >= 0.14.1 libavutil], [HAVE_LIBSWSCALE=1], [HAVE_LIBSWSCALE=0])
if test "$HAVE_LIBSWSCALE" != "1"; then
    AC_MSG_WARN([required library libswscale >= 0.14.1 not found:])
    AC_MSG_WARN([$libswscale_PKG_ERRORS])
    AC_MSG_WARN([libswscale >= 0.14.1 is provided by libav >= 0.7 or FFmpeg >= 0.7])
fi
AC_CHECK_FUNCS([sysconf])

dnl libass
LIBASS_PKGCONFIG_VERSION="\"\""
PKG_CHECK_MODULES([libass], [libass >= 0.9.9], [HAVE_LIBASS=1], [HAVE_LIBASS=0])
if test "$HAVE_LIBASS" != "1"; then
    AC_MSG_WARN([required library libass not found:])
    AC_MSG_WARN([$libass_PKG_ERRORS])
    AC_MSG_WARN([libass is provided by LibASS; Debian package: libass-dev])
else
    LIBASS_PKGCONFIG_VERSION="\"`$PKG_CONFIG --modversion libass`\""
fi
AC_DEFINE_UNQUOTED([LIBASS_PKGCONFIG_VERSION], [$LIBASS_PKGCONFIG_VERSION], [libass version])

dnl OpenAL
PKG_CHECK_MODULES([libopenal], [openal >= 0.0], [HAVE_LIBOPENAL=1], [HAVE_LIBOPENAL=0])
if test "$HAVE_LIBOPENAL" = "1"; then
    CPPFLAGS_BAK="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $libopenal_CFLAGS"
    AC_CHECK_HEADERS([AL/al.h])
    CPPFLAGS="$CPPFLAGS_BAK"
else
    AC_MSG_WARN([required library libopenal not found:])
    AC_MSG_WARN([$libopenal_PKG_ERRORS])
    AC_MSG_WARN([libopenal is provided by OpenAL; Debian package: libopenal-dev])
fi

dnl Qt
qt_version=""
AC_ARG_WITH([qt-version],
    [AS_HELP_STRING([--with-qt-version], [Choose Qt version. Currently 4 or 5; 4 is the default.])],
    [qt_version="$withval"], [qt_version="4"])
if test "$qt_version" != "4" -a "$qt_version" != "5"; then
    AC_MSG_WARN([unknown Qt version requested, falling back to Qt 4.])
    qt_version="4"
fi
AC_ARG_VAR([RCC], [rcc command])
AC_ARG_VAR([MOC], [moc command])
if test "$qt_version" = "4"; then
    PKG_CHECK_MODULES([libqtopengl], [QtOpenGL >= 4.6], [HAVE_LIBQTOPENGL=1], [HAVE_LIBQTOPENGL=0])
    if test "$HAVE_LIBQTOPENGL" != "1"; then
        AC_MSG_WARN([required library QtOpenGL not found:])
        AC_MSG_WARN([$libqtopengl_PKG_ERRORS])
        AC_MSG_WARN([QtOpenGL is provided by Qt])
    else
        if test -z "$RCC"; then
            AC_CHECK_TOOLS([RCC], [rcc-qt4 rcc])
        fi
        if test -z "$MOC"; then
            AC_CHECK_TOOLS([MOC], [moc-qt4 moc])
        fi
    fi
    dnl Check if we are running on X11. Needed for GLXEW.
    AC_MSG_CHECKING([whether Qt uses X11])
    CPPFLAGS_BAK="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $libqtopengl_CFLAGS"
    AC_COMPILE_IFELSE(
        [AC_LANG_PROGRAM([[
         #include <QGLWidget>
         #ifndef Q_WS_X11
          error: No X11
         #endif]],
        [[int x = 0;]])],
        [NEED_LIBX11="yes"],[NEED_LIBX11="no"])
    CPPFLAGS="$CPPFLAGS_BAK"
    AC_MSG_RESULT([$NEED_LIBX11])
    dnl With Qt4, we use X11 functions directly, and thus need to link with -lX11
    if test "$NEED_LIBX11" = "yes"; then
        AC_DEFINE([HAVE_X11], [1], [Whether we are running on X11])
        PKG_CHECK_MODULES([libX11], [x11 >= 0.0], [HAVE_LIBX11=1], [HAVE_LIBX11=0])
        if test "$HAVE_LIBX11" != "1"; then
            AC_MSG_WARN([required library libX11 not found:])
            AC_MSG_WARN([$libX11_PKG_ERRORS])
        else
            libqtopengl_CFLAGS="$libqtopengl_CFLAGS $libX11_CFLAGS"
            libqtopengl_LIBS="$libqtopengl_LIBS $libX11_LIBS"
        fi
    else
        AC_DEFINE([HAVE_X11], [0], [Whether we are running on X11])
    fi
else
    PKG_CHECK_MODULES([libqtopengl], [Qt5OpenGL >= 5.1], [HAVE_LIBQTOPENGL=1], [HAVE_LIBQTOPENGL=0])
    if test -z "$RCC"; then
        AC_CHECK_TOOLS([RCC], [rcc-qt5 rcc])
    fi
    if test -z "$MOC"; then
        AC_CHECK_TOOLS([MOC], [moc-qt5 moc])
    fi
    dnl Qt5 on Linux requires -fPIE / -pie with gcc, but does not record this fact
    dnl in the pkg-config files (which is a bug in my opinion). Work around that.
    AC_MSG_CHECKING([whether Qt needs -fPIE and -pie])
    if test -n "`$PKG_CONFIG --variable=qt_config Qt5Core | $GREP reduce_relocations`"; then
        AC_MSG_RESULT([yes])
        CXXFLAGS="$CXXFLAGS -fPIE"
        LDFLAGS="$LDFLAGS -pie"
    else
        AC_MSG_RESULT([no])
    fi
    dnl Check if we are running on X11. Needed for GLXEW.
    AC_MSG_CHECKING([whether Qt uses X11])
    if test -n "`$PKG_CONFIG --variable=qt_config Qt5Core | $GREP xcb`"; then
        AC_DEFINE([HAVE_X11], [1], [Whether we are running on X11])
        AC_MSG_RESULT([yes])
    else
        AC_DEFINE([HAVE_X11], [0], [Whether we are running on X11])
        AC_MSG_RESULT([no])
    fi
fi
if test "$HAVE_LIBQTOPENGL" = "1"; then
    if test -z "$RCC" -o -z "$MOC"; then
        AC_MSG_WARN([required tools rcc and/or moc not found; Qt not usable])
        HAVE_LIBQTOPENGL="0"
    fi
fi

dnl Equalizer
dnl We assume that Equalizer does not use an included GLEW!
AC_ARG_WITH([equalizer],
    [AS_HELP_STRING([--without-equalizer], [Disable multi-display support via Equalizer (enabled by default)])],
    [if test "$withval" = "yes"; then equalizer="yes"; else equalizer="no"; fi], [equalizer="yes"])
if test "$equalizer" = "yes"; then
    PKG_CHECK_MODULES([libEqualizer], [Equalizer >= 0.9.3], [HAVE_LIBEQUALIZER=1], [HAVE_LIBEQUALIZER=0])
    if test "$HAVE_LIBEQUALIZER" != "1"; then
        AC_MSG_WARN([optional library libEqualizer not found:])
        AC_MSG_WARN([$libEqualizer_PKG_ERRORS])
        AC_MSG_WARN([libEqualizer is provided by Equalizer])
	equalizer="no"
    fi
else
    HAVE_LIBEQUALIZER=0
fi
AC_DEFINE_UNQUOTED([HAVE_LIBEQUALIZER], [$HAVE_LIBEQUALIZER], [Have Equalizer?])
AM_CONDITIONAL([HAVE_LIBEQUALIZER], [test "$HAVE_LIBEQUALIZER" = "1"])

dnl glewmx
PKG_CHECK_MODULES([libglewmx], [glewmx >= 1.6.0], [HAVE_LIBGLEWMX=1], [HAVE_LIBGLEWMX=0])
if test "$HAVE_LIBGLEWMX" != "1"; then
    AC_MSG_WARN([required library libglewmx >= 1.6.0 not found:])
    AC_MSG_WARN([$libglewmx_PKG_ERRORS])
    AC_MSG_WARN([libglewmx is provided by glew; Debian package: libglewmx-dev])
fi
AM_CONDITIONAL([HAVE_LIBGLEWMX], [test "$HAVE_LIBGLEWMX" = "1"])

dnl libXNVCtrl
AC_ARG_WITH([xnvctrl],
    [AS_HELP_STRING([--with-xnvctrl], [Enable NVIDIA Quadro SDI output (disabled by default)])],
    [if test "$withval" = "yes"; then xnvctrl="yes"; else xnvctrl="no"; fi], [xnvctrl="no"])
if test "$xnvctrl" = "yes"; then
    CPPFLAGS_BAK="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $libX11_CFLAGS $xnvctrl_CFLAGS"
    AC_SUBST([xnvctrl_CFLAGS])
    AC_CHECK_HEADERS([NVCtrl.h NVCtrlLib.h],
                     [HAVE_LIBXNVCTRL=1],
                     [HAVE_LIBXNVCTRL=0],
                     [#include <GL/glx.h>])
    if test "$HAVE_LIBXNVCTRL" = "1"; then
        LDFLAGS_BAK="$LDFLAGS"
        LDFLAGS="$LDFLAGS $xnvctrl_LDFLAGS"
        AC_SUBST([xnvctrl_LDFLAGS])
        AC_CHECK_LIB([XNVCtrl],
                     [XNVCTRLQueryAttribute],
                     [HAVE_LIBXNVCTRL=1],
                     [HAVE_LIBXNVCTRL=0],
                     [$libX11_LIBS -lXext])
        LDFLAGS="$LDFLAGS_BAK"
    fi
    CPPFLAGS="$CPPFLAGS_BAK"
    if test "$HAVE_LIBXNVCTRL" != "1"; then
        AC_MSG_WARN([optional library libXNVCtrl not found])
        AC_MSG_WARN([libXNVCtrl is provided by NVIDIA; Ubuntu package: nvidia-settings])
        xnvctrl="no"
    fi
else
    HAVE_LIBXNVCTRL=0
fi
AC_DEFINE_UNQUOTED([HAVE_LIBXNVCTRL], [$HAVE_LIBXNVCTRL], [Have NVIDIA libXNVCtrl for Quadro SDI output?])
AM_CONDITIONAL([HAVE_LIBXNVCTRL], [test "$HAVE_LIBXNVCTRL" = "1"])

dnl OpenGL
dnl We need to explicitly link with OpenGL since we use its functions directly.
dnl However, this is most probably not necessary on Windows and Mac OS X (for
dnl now), so we just use a pkg-config check (which usually only works on Linux)
dnl and ignore a failure.
PKG_CHECK_MODULES([libgl], [gl >= 0.0], [HAVE_LIBGL=1], [HAVE_LIBGL=0])

dnl LIRC
LIBLIRCCLIENT_PKGCONFIG_VERSION="\"\""
AC_ARG_WITH([liblircclient],
    [AS_HELP_STRING([--without-liblircclient], [Disable remote control support via liblircclient (enabled by default)])],
    [if test "$withval" = "yes"; then lirc="yes"; else lirc="no"; fi], [lirc="yes"])
if test "$lirc" = "yes"; then
    PKG_CHECK_MODULES([liblircclient], [liblircclient0 >= 0.0], [HAVE_LIBLIRCCLIENT=1], [HAVE_LIBLIRCCLIENT=0])
    if test "$HAVE_LIBLIRCCLIENT" != "1"; then
        AC_MSG_WARN([optional library liblircclient not found:])
        AC_MSG_WARN([$liblircclient_PKG_ERRORS])
        AC_MSG_WARN([liblircclient is provided by LIRC; Debian package: liblircclient-dev])
	lirc="no"
    else
        LIBLIRCCLIENT_PKGCONFIG_VERSION="\"`$PKG_CONFIG --modversion liblircclient0`\""
    fi
else
    HAVE_LIBLIRCCLIENT=0
fi
AC_DEFINE_UNQUOTED([HAVE_LIBLIRCCLIENT], [$HAVE_LIBLIRCCLIENT], [Have liblircclient?])
AC_DEFINE_UNQUOTED([LIBLIRCCLIENT_PKGCONFIG_VERSION], [$LIBLIRCCLIENT_PKGCONFIG_VERSION], [liblircclient version])
AM_CONDITIONAL([HAVE_LIBLIRCCLIENT], [test "$HAVE_LIBLIRCCLIENT" = "1"])

dnl Icon and Menu tools. It is ok if these are missing.
GTK_UPDATE_ICON_CACHE=""
AC_ARG_VAR([GTK_UPDATE_ICON_CACHE], [gtk-update-icon-cache command])
AC_CHECK_PROGS([GTK_UPDATE_ICON_CACHE], [gtk-update-icon-cache])
UPDATE_DESKTOP_DATABASE=""
AC_ARG_VAR([UPDATE_DESKTOP_DATABASE], [update-desktop-database command])
AC_CHECK_PROGS([UPDATE_DESKTOP_DATABASE], [update-desktop-database])

dnl Check if all libraries were found
if test "$am_cv_func_iconv" != "yes" \
    -o "$HAVE_LIBAVFORMAT" != "1" \
    -o "$HAVE_LIBAVDEVICE" != "1" \
    -o "$HAVE_LIBSWSCALE" != "1" \
    -o "$HAVE_LIBASS" != "1" \
    -o "$HAVE_LIBOPENAL" != "1" \
    -o "$HAVE_LIBQTOPENGL" != "1" \
    -o "$HAVE_LIBGLEWMX" != "1" \
    -o \( "$equalizer" = "yes" -a "$HAVE_LIBEQUALIZER" != "1" \) \
    -o \( "$xnvctrl" = "yes" -a "$HAVE_LIBXNVCTRL" != "1" \) \
    -o \( "$lirc" = "yes" -a "$HAVE_LIBLIRCCLIENT" != "1" \) ; then
    AC_MSG_ERROR([One or more libraries were not found. See messages above.])
fi

dnl Output
AC_CONFIG_FILES([Makefile	\
	po/Makefile.in		\
	src/base/Makefile	\
	src/Makefile		\
	doc/Makefile		\
	pkg/macosx/Info.plist	\
	])
AC_OUTPUT

dnl Print configuration for user to check
echo "Configuration:"
echo "NLS:                      $USE_NLS"
echo "Equalizer:                $equalizer"
echo "NVIDIA Quadro SDI output: $xnvctrl"
echo "liblircclient:            $lirc"
