SUBDIRS=src
AUTOMAKE_OPTIONS=foreign

EXTRA_DIST=THANKS autogen.sh doc debian acinclude

# remove CVS files from examples, debian and acinclude
dist-hook:
	rm -rf `find $(distdir) -name CVS`

pkgdocdir=${prefix}/share/doc/@PACKAGE@

pkgdoc_DATA=COPYING NEWS README THANKS TODO doc/ABSTRACT doc/examples/mailsync doc/mailsync.1
