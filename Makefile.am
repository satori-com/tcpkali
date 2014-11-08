
SUBDIRS = src test

##
## Building RPM.
##
PKG_NAME=@PACKAGE@
PKG_VERSION=@PACKAGE_VERSION@
PKG_PREFIX=/usr/local/bin
pkgroot:: src
	rm -rf pkgroot
	mkdir pkgroot
	cp $(builddir)/src/tcpkali pkgroot
-include mz-rpm.mk
