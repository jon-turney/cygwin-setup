## Process this file with automake to produce Makefile.in
#
# $Id$
#
# Copyright (c) 2000, Red Hat, Inc.
# Copyright (c) 2002, Robert Collins
#
#     This program is free software; you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation; either version 2 of the License, or
#     (at your option) any later version.
#
#     A copy of the GNU General Public License can be found at
#     http://www.gnu.org/
#
# Written by Christopher Faylor <cgf@redhat.com>
# and Robert Collins  <rbtcollins@hotmail.com>
#
# Makefile for Cygwin installer

AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects 1.6
SUBDIRS = bz2lib zlib

## DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]*

AM_CFLAGS	= -Werror -Winline -Wall -Wpointer-arith \
  -Wcast-align -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
  -Wmissing-declarations -Wcomments -mwindows
AM_CXXFLAGS	= $(AM_CFLAGS) -fno-rtti

WINDRES		:= @WINDRES@

INCLUDES	= -I$(srcdir)/bz2lib

noinst_PROGRAMS = setup

# to avoid false errors that assembly generates
WARNONLY_CFLAGS	= -Winline -Wall -Wpointer-arith -Wcast-align \
	-Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes \
	-Wmissing-declarations -Wcomments $(INCLUDES) -mwindows

BUILT_SOURCES = setup_version.c \
  	inilex.cc \
	iniparse.cc

setup_LDADD = zlib/libzcygw.a bz2lib/libbz2.a -lmingw32 -luser32 -lkernel32 \
  -lcomctl32 -lole32 -lwsock32 -lnetapi32 -ladvapi32 -luuid
setup_SOURCES = \
	archive.cc \
	archive_tar.cc \
	archive_tar_file.cc \
	autoload.c \
	category.cc \
	choose.cc \
	cistring.cc \
	compress.cc \
	compress_bz.cc \
	compress_gz.cc \
	concat.cc \
	cygpackage.cc \
	desktop.cc \
	dialog.cc \
	diskfull.cc \
	download.cc \
	find.cc \
	filemanip.cc \
	fromcwd.cc \
	GetOption.cc \
	geturl.cc \
	hash.cc \
	ini.cc \
	inilex.cc \
	iniparse.cc \
	iniparse.h \
	install.cc \
	io_stream.cc \
	io_stream_cygfile.cc \
	io_stream_file.cc \
	io_stream_memory.cc \
	localdir.cc \
	log.cc \
	main.cc \
	getopt.c \
	mkdir.cc \
	mklink2.cc \
	mount.cc \
	msg.cc \
	net.cc \
	netio.cc \
	nio-ie5.cc \
	nio-file.cc \
	nio-ftp.cc \
	nio-http.cc \
	Option.cc \
	package_db.cc \
	package_meta.cc \
	package_source.cc \
	package_version.cc \
	PickCategoryLine.cc \
	PickLine.cc \
	PickPackageLine.cc \
	PickView.cc \
	postinstall.cc \
	proppage.cc \
	propsheet.cc \
	res.rc \
	rfc1738.cc \
	root.cc \
	script.cc \
	setup_version.c \
	simpsock.cc \
	site.cc \
	source.cc \
	splash.cc \
	state.cc \
	String++.cc \
	threebar.cc \
	version.cc \
	win32.cc \
	window.cc
	
setup_version.c : $(srcdir)/ChangeLog Makefile
	V=`grep '^\$$Revision.*' $(srcdir)/ChangeLog \
	| sed -e 's/\$$Revision:* *//' \
	  -e 's/ *$$.*//'` ;\
	(echo '#define VERSION_PREFIX "%%% setup-version"';\
        echo 'static const char version_store[] = VERSION_PREFIX " '$$V'";';\
        echo 'const char *version = version_store + sizeof (VERSION_PREFIX);') > version.tmp;\
	mv version.tmp setup_version.c

%.o: %.rc
	$(WINDRES) --include-dir $(srcdir) -o $@ $<

	
## add in dependencies from depcomp
@AMDEP_TRUE@@am__include@ @am__quote@$(DEPDIR)/autoload.Po@am__quote@
@AMDEP_TRUE@@am__include@ @am__quote@$(DEPDIR)/inilex.Po@am__quote@

autoload.o: autoload.c
@AMDEP_TRUE@    source='$<' object='$@' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@    depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' @AMDEPBACKSLASH@
@AMDEP_TRUE@    $(CDEPMODE) $(depcomp) @AMDEPBACKSLASH@
	$(CC) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) $(CPPFLAGS) $(WARNONLY_CFLAGS) $(CFLAGS) -c -o $@ $< -fno-inline-functions

inilex.o: inilex.cc
@AMDEP_TRUE@    source='$<' object='$@' libtool=no @AMDEPBACKSLASH@
@AMDEP_TRUE@    depfile='$(DEPDIR)/$*.Po' tmpdepfile='$(DEPDIR)/$*.TPo' @AMDEPBACKSLASH@
@AMDEP_TRUE@    $(CXXDEPMODE) $(depcomp) @AMDEPBACKSLASH@
	$(CXX) $(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) \
       $(AM_CPPFLAGS) $(CPPFLAGS) $(WARNONLY_CFLAGS) $(CXXFLAGS) -fno-rtti -c -o $@ $< 

iniparse.cc iniparse.h : iniparse.y
	bison -d -o iniparse.cc $(srcdir)/iniparse.y
	@mv iniparse.cc.h iniparse.h 2>/dev/null || mv iniparse.hh iniparse.h

inilex.cc : inilex.l iniparse.h
	flex -8 $(srcdir)/inilex.l
	mv lex.yy.c inilex.cc
