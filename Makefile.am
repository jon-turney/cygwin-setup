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
# Makefile for Cygwin installer

SUBDIRS := @subdirs@ tests

## DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]*

# We would like to use -Winline for C++ as well, but some STL code triggers
# this warning. (Bug verified present in gcc-3.3)
# -Wno-uninitialized added to deal with g++ 3.4.4's spurious STL warnings
# (http://gcc.gnu.org/bugzilla/show_bug.cgi?id=22207)
BASECXXFLAGS = -Wall -Wno-uninitialized -Wpointer-arith -Wcomments \
	       -Wcast-align -Wwrite-strings -fno-builtin-sscanf \
	       -Wno-attributes
AM_CXXFLAGS = -Werror $(BASECXXFLAGS) ${$(*F)_CXXFLAGS}
AM_CFLAGS = $(AM_CXXFLAGS) -Wmissing-declarations -Winline \
	    -Wstrict-prototypes -Wmissing-prototypes
AM_YFLAGS = -d
AM_LFLAGS = -8
WINDRES = @WINDRES@
AM_CPPFLAGS = -DLZMA_API_STATIC -I$(srcdir)/libgetopt++/include -I${srcdir}/regex

inilex_CXXFLAGS:=-Wno-sign-compare

noinst_PROGRAMS = @SETUP@$(EXEEXT) @INILINT@

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
	cygwin-setup.ico \
	cygwin-terminal.ico \
	setup.exe.manifest \
	setup64.exe.manifest \
	tree-minus.bmp \
	tree-plus.bmp

# iniparse.hh is generated from iniparse.yy via bison -d, so it needs to be
# included here for proper tracking (but not iniparse.cc, since automake
# knows about that already)
BUILT_SOURCES = \
	setup_version.c \
	iniparse.hh

CLEANFILES = setup_version.c

inilint_LDADD = \
	libgetopt++/libgetopt++.la
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
	inilex.ll \
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
	mklink2.cc \
	PackageSpecification.cc \
	PackageSpecification.h \
	PackageTrust.h \
	csu_util/rfc1738.cc \
	csu_util/rfc1738.h \
	String++.cc \
	String++.h

@SETUP@_LDADD = \
	libgetopt++/libgetopt++.la -lgcrypt -lgpg-error -llzma -lbz2 -lz \
	-lshlwapi -lcomctl32 -lole32 -lwsock32 -lpsapi -luuid -lntdll -lwininet -lmingw32
@SETUP@_LDFLAGS = -mwindows -Wc,-static -static-libtool-libs
@SETUP@_SOURCES = \
	AntiVirus.cc \
	AntiVirus.h \
	archive.cc \
	archive.h \
	archive_tar.cc \
	archive_tar.h \
	archive_tar_file.cc \
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
	inilex.ll \
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
	postinstallresults.cc \
	postinstallresults.h \
	prereq.cc \
	prereq.h \
	processlist.cc \
	processlist.h \
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
	sha2.h \
	sha2.c \
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
	regex/regex.c

VER := $(shell cd ${srcdir} && git rev-list master --count ChangeLog)

setup_version.c : $(srcdir)/ChangeLog Makefile
	$(AM_V_GEN)(echo '#define VERSION_PREFIX "%%% setup-version"';\
	echo 'static const char version_store[] = VERSION_PREFIX " '${VER}'";';\
	echo 'const char *setup_version = version_store + sizeof (VERSION_PREFIX);') > version.tmp && \
	mv version.tmp setup_version.c

# setup.exe.manifest is included in res.rc
res.o: @SETUP@.exe.manifest

.rc.o:
	$(AM_V_GEN)$(WINDRES) --include-dir $(srcdir) -o $@ $<

# static const char version_store[] = VERSION_PREFIX " 2.686";
setup-src:
	@ver=setup-$$(sed -n 's/^static const char version_store.* VERSION_PREFIX " \([^"]*\)".*$$/\1/p' setup_version.c);\
	cd ${srcdir};\
	rm -f $$ver;\
	ln -sf . $$ver;\
	cvs status -R | sed -n "s%^ *Repository revision:.*/cvs/cygwin-apps/setup/\(.*\),v%$$ver/\1%p" |\
	sort | tar -T - -cjf ${CURDIR}/$$ver-src.tar.bz2;\
	echo $$ver-src.tar.bz2; exec rm -f $$ver

# optional: strip and compress executable
.PHONY:	strip upx

strip:	all
	$(STRIP) -s setup$(EXEEXT)
upx:	strip
	@if [ -e `which upx` ]; then\
		upx --best --lzma setup$(EXEEXT) ;\
	else \
		echo "UPX doesn't seem to be installed, cannot compress setup$(EXEEXT)." ;\
	fi
