## Process this file with automake to produce Makefile.in.

AUTOMAKE_OPTIONS = cygnus

libzcygw_a_SOURCES = adler32.c compress.c crc32.c deflate.c deflate.h \
gzio.c infblock.c infblock.h infcodes.c infcodes.h inffast.c inffast.h \
inffixed.h inflate.c inftrees.c inftrees.h infutil.c infutil.h trees.c \
trees.h uncompr.c zconf.h zlib.h zutil.c zutil.h

tooldir = $(exec_prefix)/$(host_alias)

INCLUDES = -I@srcdir@/..

noinst_LIBRARIES = @target_all@
EXTRA_LIBRARIES = libzcygw.a

install-data-local:
	$(INSTALL_DATA) $(srcdir)/zcygw $(sysconfdir)/zcygw
	$(INSTALL_DATA) $(srcdir)/zcygw.h $(tooldir)/include/zcygw.h

install-exec-local: $(noinst_LIBRARIES)
	$(INSTALL) libzcygw.a $(tooldir)/lib/libzcygw.a
