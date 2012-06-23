# Copyright (C) 2010-2011
# Martin Lambers <marlam@marlam.de>
# Frédéric Devernay <Frederic.Devernay@inrialpes.fr>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

SUBDIRS = po src doc

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = README.Linux README.FreeBSD README.MacOSX README.Windows

# Create the ChangeLog file from the git log
dist-hook:
	if test -d "$(srcdir)/.git" && type git > /dev/null 2>&1; then \
		(cd "$(srcdir)" && git log --date=iso --stat) > "$(distdir)/ChangeLog"; \
	fi

# This works with GNU tar and gives cleaner package than normal 'make dist'.
# (Taken from xz-4.999.9beta/Makefile.am)
mydist:
	umask 022; \
	TAR_OPTIONS='--owner=0 --group=0 --numeric-owner --mode=u+rw,go+r-w' $(MAKE) dist-xz

# Make a W32 installer using NSIS
EXTRA_DIST += pkg/w32/bino.nsi pkg/w32/notes.txt
if HAVE_MAKENSIS
package-w32:
	$(MAKE) -C po update-po && \
	TMPDIR="`mktemp -d`" && \
	$(MAKE) install-strip prefix="$$TMPDIR" bindir="$$TMPDIR" docdir="$$TMPDIR/doc" localedir="$$TMPDIR" && \
	cp $(top_srcdir)/pkg/w32/bino.nsi "$$TMPDIR" && \
	cp $(top_srcdir)/pkg/w32/notes.txt "$$TMPDIR" && \
	cp $(top_srcdir)/COPYING "$$TMPDIR" && \
	cp $(top_srcdir)/src/logo/bino_logo.ico "$$TMPDIR" && \
	$(MAKENSIS) -DPACKAGE_VERSION="$(PACKAGE_VERSION)" "$$TMPDIR/bino.nsi" && \
	cp "$$TMPDIR/bino-$(PACKAGE_VERSION)-w32.exe" . && \
	chmod 644 bino-$(PACKAGE_VERSION)-w32.exe && \
	rm -rf "$$TMPDIR"
endif

# Make a package for Mac OS X
EXTRA_DIST += pkg/macosx/README pkg/macosx/Bino.icns
package-macosx:
	TMPDIR="`mktemp -d /tmp/temp.XXXX`" && \
	BINO_APP="$$TMPDIR/Bino.app" && \
	$(INSTALL) -d "$$BINO_APP/Contents/MacOS" "$$BINO_APP/Contents/Resources/Bino Help" && \
	$(INSTALL) -m 755 src/bino "$$BINO_APP/Contents/MacOS/Bino" && \
	$(INSTALL) -m 644 $(top_srcdir)/pkg/macosx/Bino.icns "$$BINO_APP/Contents/Resources" && \
	$(INSTALL) -m 644 $(top_srcdir)/pkg/macosx/Info.plist "$$BINO_APP/Contents" && \
	$(MAKE) -C doc install-dist_htmlDATA htmldir="$$BINO_APP/Contents/Resources/Bino Help" && \
	$(MAKE) -C po install-strip localedir="$$BINO_APP/Contents/Resources/locale" && \
	macdeployqt "$$BINO_APP" && \
	rm -rf Bino.app && \
	ditto "$$BINO_APP" Bino.app && \
	rm -rf "$$TMPDIR"
