
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = asn1 doc deps src test

if GNU_STYLE_OPTIONAL_INCLUDE
##
## Building MZ-specific RPM.
##
PKG_NAME=@PACKAGE@
PKG_VERSION=@PACKAGE_VERSION@
PKG_PREFIX=/usr/local
.PHONY: pkgroot
pkgroot:: src
	rm -rf pkgroot
	mkdir -p pkgroot/bin pkgroot/share/man/man1
	cp $(builddir)/src/tcpkali pkgroot/bin
	cp $(srcdir)/doc/tcpkali.1 pkgroot/share/man/man1
-include mz-rpm.mk
endif
