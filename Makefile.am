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

INST_SUBDIRS:=@subdirs@
DIST_SUBDIRS:=${INST_SUBDIRS} tests
SUBDIRS:=tests

## DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]*

# We would like to use -Winline for C++ as well, but some STL code triggers
# this warning. (Bug verified present in gcc-3.3)
# -Wno-uninitialized added to deal with g++ 3.4.4's spurious STL warnings
# (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=22207)
BASECXXFLAGS = -Wall -Wno-uninitialized -Wpointer-arith -Wcomments \
               -Wcast-align -Wwrite-strings
AM_CXXFLAGS = -Werror $(BASECXXFLAGS)
AM_CFLAGS = $(AM_CXXFLAGS) -Wmissing-declarations -Winline \
            -Wstrict-prototypes -Wmissing-prototypes
AM_YFLAGS = -d
AM_LFLAGS = -8
WINDRES = @WINDRES@
AM_CPPFLAGS = -DLZMA_API_STATIC -I$(srcdir)/libgetopt++/include

noinst_PROGRAMS = setup @INILINT@

EXTRA_PROGRAMS = inilint
## noinst_PROGRAMS +=inilint

EXTRA_DIST = \
	CHANGES \
	CONTRIBUTORS \
	COPYING \
	bootstrap.sh \
	check-na.bmp \
	check-no.bmp \
	check-yes.bmp \
	choose-rtarrow.bmp \
	choose-spin.bmp \
	cygwin.ico \
	doconfigure \
	libmd5-rfc/README \
	libmd5-rfc/md5main.c \
	setup.exe.manifest \
	tree-minus.bmp \
	tree-plus.bmp

# iniparse.h is generated from iniparse.yy via bison -d, so it needs to be
# included here for proper tracking (but not iniparse.cc, since automake
# knows about that already)
BUILT_SOURCES = \
	setup_version.c \
	iniparse.h \
	${INST_SUBDIRS}

CLEANFILES = setup_version.c

if MINGWTARGET
inilint_extras = \
	autoload.c \
	mklink2.cc
else
inilint_extras =
endif

inilint_LDADD = \
	libinilex.a libgetopt++/libgetopt++.la
inilint_SOURCES = \
	filemanip.cc \
	filemanip.h \
	find.cc \
	find.h \
	FindVisitor.cc \
	FindVisitor.h \
	LogSingleton.cc \
	LogSingleton.h \
	IniDBBuilder.h \
	inilintmain.cc \
	iniparse.yy \
	IniParseFeedback.cc \
	IniParseFeedback.h \
	io_stream.h \
	io_stream.cc \
	io_stream_file.h \
	io_stream_file.cc \
	IOStreamProvider.h \
	mkdir.cc \
	mkdir.h \
	PackageSpecification.cc \
	PackageSpecification.h \
	PackageTrust.h \
	csu_util/rfc1738.cc \
	csu_util/rfc1738.h \
	String++.cc \
	String++.h \
	$(inilint_extras)

# workaround to allow omitting -Werror on inilex.cc.
noinst_LIBRARIES = libinilex.a
libinilex_a_SOURCES = inilex.ll
libinilex_a_CXXFLAGS = $(BASECXXFLAGS)

setup_LDADD = \
	libinilex.a \
	-Linst/lib -lgetopt++ -lgcrypt -lgpg-error \
	-lshlwapi -lcomctl32 -lole32 -lwsock32 -lnetapi32 -luuid -llzma -lbz2 -lz 
setup_LDFLAGS = -mwindows -Wl,-static -static-libtool-libs
setup_SOURCES = \
	AntiVirus.cc \
	AntiVirus.h \
	archive.cc \
	archive.h \
	archive_tar.cc \
	archive_tar.h \
	archive_tar_file.cc \
	autoload.c \
	choose.cc \
	choose.h \
	compress.cc \
	compress.h \
	compress_bz.cc \
	compress_bz.h \
	compress_gz.cc \
	compress_gz.h \
	compress_xz.cc \
	compress_xz.h \
	ConnectionSetting.cc \
	ConnectionSetting.h \
	ControlAdjuster.cc \
	ControlAdjuster.h \
	crypto.cc \
	crypto.h \
	cyg-pubkey.h \
	cygpackage.cc \
	cygpackage.h \
	desktop.cc \
	desktop.h \
	dialog.cc \
	dialog.h \
	diskfull.cc \
	diskfull.h \
	download.cc \
	download.h \
	Exception.cc \
	Exception.h \
	find.cc \
	find.h \
	FindVisitor.cc \
	FindVisitor.h \
	filemanip.cc \
	filemanip.h \
	fromcwd.cc \
	Generic.h \
	geturl.cc \
	geturl.h \
	gpg-packet.cc \
	gpg-packet.h \
	ini.cc \
	ini.h \
	IniDBBuilder.h \
	IniDBBuilderPackage.cc \
	IniDBBuilderPackage.h \
	iniparse.yy \
	IniParseFeedback.cc \
	IniParseFeedback.h \
	IniParseFindVisitor.cc \
	IniParseFindVisitor.h \
	install.cc \
	io_stream.cc \
	io_stream.h \
	io_stream_cygfile.cc \
	io_stream_cygfile.h \
	io_stream_file.cc \
	io_stream_file.h \
	io_stream_memory.cc \
	io_stream_memory.h \
	IOStreamProvider.h \
	KeysSetting.cc \
	KeysSetting.h \
	localdir.cc \
	localdir.h \
	LogFile.cc \
	LogFile.h \
	LogSingleton.cc \
	LogSingleton.h \
	main.cc \
	mkdir.cc \
	mkdir.h \
	mklink2.cc \
	mklink2.h \
	mount.cc \
	mount.h \
	msg.cc \
	msg.h \
	net.cc \
	net.h \
	netio.cc \
	netio.h \
	nio-ie5.cc \
	nio-ie5.h \
	nio-file.cc \
	nio-file.h \
	nio-ftp.cc \
	nio-ftp.h \
	nio-http.cc \
	nio-http.h \
	package_db.cc \
	package_db.h \
	package_meta.cc \
	package_meta.h \
	package_source.cc \
	package_source.h \
	package_version.cc \
	package_version.h \
	PackageSpecification.cc \
	PackageSpecification.h \
	PackageTrust.h \
	PickCategoryLine.cc \
	PickCategoryLine.h \
	PickLine.h \
	PickPackageLine.cc \
	PickPackageLine.h \
	PickView.cc \
	PickView.h \
	postinstall.cc \
	prereq.cc \
	prereq.h \
	proppage.cc \
	proppage.h \
	propsheet.cc \
	propsheet.h \
	RECTWrapper.h \
	res.rc \
	resource.h \
	root.cc \
	root.h \
	ScanFindVisitor.cc \
	ScanFindVisitor.h \
	script.cc \
	script.h \
	setup_version.h \
	setup_version.c \
	simpsock.cc \
	simpsock.h \
	site.cc \
	site.h \
	source.cc \
	source.h \
	SourceSetting.cc \
	SourceSetting.h \
	splash.cc \
	splash.h \
	state.cc \
	state.h \
	String++.cc \
	String++.h \
	threebar.cc \
	threebar.h \
	UserSettings.cc \
	UserSettings.h \
	win32.cc \
	win32.h \
	window.cc \
	window.h \
	csu_util/MD5Sum.cc \
	csu_util/MD5Sum.h \
	csu_util/rfc1738.cc \
	csu_util/rfc1738.h \
	csu_util/version_compare.cc \
	csu_util/version_compare.h \
	libmd5-rfc/md5.c \
	libmd5-rfc/md5.h

VER := $(shell sed -ne 's/^\$$Revi[s]ion: *\([^ ]*\) *$$.*/\1/p' \
               $(srcdir)/ChangeLog)

setup_version.c : $(srcdir)/ChangeLog Makefile
	(echo '#define VERSION_PREFIX "%%% setup-version"';\
	echo 'static const char version_store[] = VERSION_PREFIX " '${VER}'";';\
	echo 'const char *setup_version = version_store + sizeof (VERSION_PREFIX);') > version.tmp
	mv version.tmp setup_version.c

.rc.o:
	$(WINDRES) --include-dir $(srcdir) -o $@ $<

# this target creates:
#   setup-x.yyy.exe             (UPXed stripped exe)
#   setup-debug-x.yyy.exe.gz    (gzipped unstripped exe)
#   setup-x.yyy.tar.bz2         (source)
release: setup.exe $(srcdir)/ChangeLog Makefile
	$(MAKE) dist-bzip2 distdir=setup-${VER}
	cp -pf setup.exe setup-debug-${VER}.exe
	gzip -f9 setup-debug-${VER}.exe
	cp -pf setup.exe setup-${VER}.exe
	$(STRIP) setup-${VER}.exe
	upx --best setup-${VER}.exe

# Create a snapshot and upload it (requires write access)
snapshot: release
	scp -C setup-${VER}.exe setup-debug-${VER}.exe.gz setup-${VER}.tar.bz2 \
	    $${cygwinsite:-cygwin.com}:setup-snapshots/

# static const char version_store[] = VERSION_PREFIX " 2.686";
setup-src:
	@ver=setup-$$(sed -n 's/^static const char version_store.* VERSION_PREFIX " \([^"]*\)".*$$/\1/p' setup_version.c);\
	cd ${srcdir};\
	rm -f $$ver;\
	ln -sf . $$ver;\
	cvs status -R | sed -n "s%^ *Repository revision:.*/cvs/cygwin-apps/setup/\(.*\),v%$$ver/\1%p" |\
	sort | tar -T - -cjf ${CURDIR}/$$ver.tar.bz2;\
	echo $$ver.tar.bz2

.PHONY: ${INST_SUBDIRS}
${INST_SUBDIRS}:
	${MAKE} -C $@ install
