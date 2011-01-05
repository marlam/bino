# Copyright (C) 2010-2011
# Martin Lambers <marlam@marlam.de>
#
# Copying and distribution of this file, with or without modification, are
# permitted in any medium without royalty provided the copyright notice and this
# notice are preserved. This file is offered as-is, without any warranty.

SUBDIRS = src doc

EXTRA_DIST = README.MacOSX README.Windows

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
	TMPDIR="`mktemp -d`" && \
	$(MAKE) -C src install-strip prefix="$$TMPDIR" bindir="$$TMPDIR" && \
	cp $(top_srcdir)/pkg/w32/bino.nsi "$$TMPDIR" && \
	cp $(top_srcdir)/pkg/w32/notes.txt "$$TMPDIR" && \
	cp $(top_srcdir)/COPYING "$$TMPDIR" && \
	cp $(top_srcdir)/src/icons/appicon.ico "$$TMPDIR" && \
	$(MAKENSIS) -DPACKAGE_VERSION="$(PACKAGE_VERSION)" "$$TMPDIR/bino.nsi" && \
	cp "$$TMPDIR/bino-$(PACKAGE_VERSION)-w32.exe" . && \
	chmod 644 bino-$(PACKAGE_VERSION)-w32.exe && \
	rm -rf "$$TMPDIR"
endif
