
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = doc deps src test

if GNU_STYLE_OPTIONAL_INCLUDE
##
## Building MZ-specific RPM.
##
PKG_NAME=@PACKAGE@
PKG_VERSION=@PACKAGE_VERSION@
PKG_PREFIX=/usr/local/bin
pkgroot:: src
	rm -rf pkgroot
	mkdir pkgroot
	cp $(builddir)/src/tcpkali pkgroot
-include mz-rpm.mk
endif
