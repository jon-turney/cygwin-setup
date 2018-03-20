## Process this file with automake to produce Makefile.in
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
BASECXXFLAGS = -Werror -Wall -Wpointer-arith -Wcomments \
	       -Wcast-align -Wwrite-strings -fno-builtin-sscanf \
	       -Wno-attributes
AM_CXXFLAGS = $(BASECXXFLAGS) -std=gnu++11 ${$(*F)_CXXFLAGS}
AM_CFLAGS = $(BASECXXFLAGS) -Wmissing-declarations -Winline \
	    -Wstrict-prototypes -Wmissing-prototypes
AM_YFLAGS = -d
AM_LFLAGS = -8
WINDRES = @WINDRES@
AM_CPPFLAGS = -DLZMA_API_STATIC -I$(srcdir)/libgetopt++/include

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
	libgetopt++/libgetopt++.la -lgcrypt -lgpg-error -llzma -lbz2 -lz -lsolv -lregex \
	-lshlwapi -lcomctl32 -lole32 -lpsapi -luuid -lntdll -lwininet -lws2_32 -lmingw32
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
	confirm.cc \
	confirm.h \
	ConnectionSetting.cc \
	ConnectionSetting.h \
	ControlAdjuster.cc \
	ControlAdjuster.h \
	crypto.cc \
	crypto.h \
	cyg-pubkey.h \
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
	libsolv.cc \
	libsolv.h \
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
	package_db.cc \
	package_db.h \
	package_depends.h \
	package_depends.cc \
	package_meta.cc \
	package_meta.h \
	package_source.cc \
	package_source.h \
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
	script.cc \
	script.h \
	setup_version.h \
	setup_version.c \
	sha2.h \
	sha2.c \
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
	csu_util/version_compare.h

GITVER := $(shell cd $(srcdir) && git describe --match release_\* --abbrev=6 --dirty || "N/A")
VER := $(subst release_,,$(GITVER))
ARCH := @ARCH@

setup_version.c : Makefile
	@echo "Setup version: " $(VER)
	$(AM_V_GEN)(echo '#define VERSION_PREFIX "%%% setup-version"';\
	echo 'static const char version_store[] = VERSION_PREFIX " '$(VER)'";';\
	echo 'const char *setup_version = version_store + sizeof (VERSION_PREFIX);') > version.tmp && \
	mv version.tmp setup_version.c

# setup.exe.manifest is included in res.rc
res.o: @SETUP@.exe.manifest

.rc.o:
	$(AM_V_GEN)$(WINDRES) --include-dir $(srcdir) -o $@ $<

setup-src:
	@ver=setup-$(VER);\
	cd ${srcdir}; rm -f $$ver; ln -sf . $$ver;\
	git ls-files | tar -T - -cJf ${CURDIR}/$$ver-src.tar.xz;\
	echo $$ver-src.tar.xz; exec rm -f $$ver

# optional: strip and compress executable
.PHONY:	strip upx

strip:	all
	$(OBJCOPY) --add-gnu-debuglink=/dev/null --only-keep-debug setup$(EXEEXT) setup.dbg
	$(OBJCOPY) --strip-all setup$(EXEEXT)
	$(OBJCOPY) --add-gnu-debuglink=setup.dbg setup$(EXEEXT) setup$(EXEEXT)

CLEANFILES += setup.dbg

upx:	strip
	@if [ -e `which upx` ]; then\
		upx --best --lzma setup$(EXEEXT) ;\
	else \
		echo "UPX doesn't seem to be installed, cannot compress setup$(EXEEXT)." ;\
	fi

release: upx
	cp setup.dbg setup-${VER}.${ARCH}.dbg
	cp setup${EXEEXT} setup-${VER}.${ARCH}${EXEEXT}

UPLOAD_HOST=cygwin-admin@cygwin.com
UPLOAD_PATH=/www/sourceware/htdocs/cygwin/setup

upload: release
	scp setup-${VER}.$(ARCH).exe setup-${VER}.$(ARCH).dbg ${UPLOAD_HOST}:${UPLOAD_PATH}
	ssh ${UPLOAD_HOST} gpg --detach-sign ${UPLOAD_PATH}/setup-${VER}.$(ARCH).exe

clean-local:
	rm -f setup*${EXEEXT} setup*.dbg
