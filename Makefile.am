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
AM_CPPFLAGS = -D__USE_MINGW_ANSI_STDIO=1 -D_FILE_OFFSET_BITS=64 -DLZMA_API_STATIC \
	       -I$(srcdir)/libgetopt++/include \
	       $(ZLIB_CFLAGS) $(LZMA_CFLAGS) $(ZSTD_CFLAGS) $(LIBCRYPT_CFLAGS) $(LIBSOLV_CFLAGS)

inilex_CXXFLAGS:=-Wno-sign-compare
iniparse_CXXFLAGS:=-Wno-free-nonheap-object

noinst_PROGRAMS = @SETUP@$(EXEEXT) inilint

EXTRA_DIST = \
	CHANGES \
	CONTRIBUTORS \
	COPYING \
	bootstrap.sh \
	cygwin.ico \
	cygwin-setup.ico \
	cygwin-terminal.ico \
	setup.exe.manifest \
	tree-minus.ico \
	tree-plus.ico

# iniparse.hh is generated from iniparse.yy via bison -d, so it needs to be
# included here for proper dependency tracking (but not iniparse.cc, since
# automake knows about that already)
BUILT_SOURCES = \
	setup_version.c \
	iniparse.hh

CLEANFILES = setup_version.c
MAINTAINERCLEANFILES = iniparse.hh

inilint_LDADD = \
	libgetopt++/libgetopt++.la \
	-lntdll -luuid

inilint_SOURCES = \
	filemanip.cc \
	filemanip.h \
	CliParseFeedback.cc \
	CliParseFeedback.h \
	LogSingleton.cc \
	LogSingleton.h \
	IniDBBuilder.h \
	inilintmain.cc \
	inilex.ll \
	iniparse.yy \
	io_stream.cc \
	io_stream.h \
	io_stream_file.cc \
	io_stream_file.h \
	mkdir.cc \
	mkdir.h \
	mklink2.cc \
	mklink2.h \
	PackageTrust.h \
	String++.cc \
	String++.h \
	win32.cc \
	win32.h

# Do not link directly with wininet, as it's vulnerable to sideloading/dll
# hijacking. Instead we make and link with a delay-loading stub lib, so it's
# actually loaded after we've had a chance to call SetDefaultDllDirectories().
#
# (The voodoo here is to grovel over the libwininet.a we would link with to
# generate the .def file, rather than having to keep our own copy around)
wininet-delaylib.a:
	$(AM_V_at)IMPLIB=$(shell $(CC) -print-file-name=libwininet.a) && \
	echo "LIBRARY" $$($(DLLTOOL) --identify $$IMPLIB) >wininet.def && \
	echo "EXPORTS" >> wininet.def && \
	$(NM) -g --defined-only $$IMPLIB | grep ' T ' | cut -d' ' -f3 >>wininet.def
	$(AM_V_GEN)$(DLLTOOL) --no-leading-underscore --input-def wininet.def --output-delaylib $@

CLEANFILES += wininet-delaylib.a wininet.def

# Unfortunately, the delay-loading stub lib doesn't work properly on 32-bit
# currently, so don't bother (since we're probably going to stop building that
# fairly soon)
#
# (See https://sourceware.org/bugzilla/show_bug.cgi?id=14339)
if ARCH_X86
WININET=-lwininet
else
WININET=wininet-delaylib.a
EXTRA_@SETUP@_DEPENDENCIES=wininet-delaylib.a
endif

@SETUP@_LDADD = \
	libgetopt++/libgetopt++.la \
	$(LIBGCRYPT_LIBS) \
	$(ZSTD_LIBS) \
	$(LZMA_LIBS) \
	$(BZ2_LIBS) \
	$(ZLIB_LIBS) \
	$(LIBSOLV_LIBS) -lregex \
	-lmingwex \
	-lshlwapi -lcomctl32 -lole32 -lpsapi -luuid -lntdll $(WININET) -lws2_32 \
	-lmingw32 -lssp
@SETUP@_LDFLAGS = -mwindows -Wc,-static -static-libtool-libs
@SETUP@_SOURCES = \
	actionlist.h \
	AntiVirus.cc \
	AntiVirus.h \
	archive.cc \
	archive.h \
	archive_tar.cc \
	archive_tar.h \
	archive_tar_file.cc \
	choose.cc \
	choose.h \
	compactos.cc \
	compactos.h \
	compress.cc \
	compress.h \
	compress_bz.cc \
	compress_bz.h \
	compress_gz.cc \
	compress_gz.h \
	compress_xz.cc \
	compress_xz.h \
	compress_zstd.cc \
	compress_zstd.h \
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
	ListView.cc \
	ListView.h \
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

GITVER := $(shell cd $(srcdir) && git describe --match release_\* --abbrev=6 --dirty || echo "N/A")
VER := $(subst release_,,$(GITVER))
ARCH := @ARCH@

setup_version.c : Makefile
	@echo "Setup version: " $(VER)
	$(AM_V_GEN)(echo '#define VERSION_PREFIX "%%% setup-version"';\
	echo 'static const char version_store[] = VERSION_PREFIX " '$(VER)'";';\
	echo 'const char *setup_version = version_store + sizeof (VERSION_PREFIX);') > version.tmp && \
	mv version.tmp setup_version.c

# rules for translation maintenance
.PHONY: pot rc2po po2rc

pot:
	cd $(srcdir) && rc2po -P res/en/res.rc res.pot

LINGUAS=de fr ja pl tr zh_Hans

rc2po:
	cd $(srcdir) && \
	for l in $(LINGUAS) ; do \
		rc2po -t res/en/res.rc --charset=utf-8 res/$$l/res.rc po/$$l/ ; \
	done

po2rc:
	cd $(srcdir) && \
	for l in $(LINGUAS) ; do \
		./langopts $$l >/dev/null && \
		$(MKDIR_P) res/$$l/ && \
		po2rc -t res/en/res.rc --charset-output=utf-8 $$(./langopts $$l) po/$$l/res.po res/$$l/res.rc && \
		d2u -q res/$$l/res.rc && \
		./rc-postprocess $$l res/$$l/res.rc ; \
	done

# files included by res.rc
res.o: setup.exe.manifest res/en/res.rc $(foreach l,$(LINGUAS),res/$l/res.rc)

.rc.o:
	$(AM_V_GEN)$(WINDRES) --codepage 65001 --include-dir $(srcdir) -o $@ $<

setup-src:
	@ver=setup-$(VER);\
	cd ${srcdir}; rm -f $$ver; ln -sf . $$ver;\
	git ls-files | tar -T - -cJf ${CURDIR}/$$ver-src.tar.xz;\
	echo $$ver-src.tar.xz; exec rm -f $$ver

# optional: strip, compress and sign executable
.PHONY:	strip upx release sign upload

strip:	all
	@$(MKDIR_P) stripped
	$(OBJCOPY) --add-gnu-debuglink=/dev/null --only-keep-debug setup$(EXEEXT) stripped/setup.dbg
	$(OBJCOPY) --strip-all setup$(EXEEXT) stripped/setup$(EXEEXT)
	$(OBJCOPY) --add-gnu-debuglink=stripped/setup.dbg stripped/setup$(EXEEXT) stripped/setup$(EXEEXT)

upx:	strip
	@if [ -e `which upx` ]; then\
		$(MKDIR_P) upx ;\
		rm -f upx/setup$(EXEEXT) ;\
		upx --best --lzma stripped/setup$(EXEEXT) -o upx/setup$(EXEEXT) ;\
		cp -a stripped/setup${EXEEXT} upx/setup-uncompressed${EXEEXT} ;\
	else \
		echo "UPX doesn't seem to be installed, cannot compress setup$(EXEEXT)." ;\
	fi

# PKCS#11 module for HSM containing signing key
PKCSMODULE=/usr/lib64/libykcs11.so.2
# PKCS#11 URI to locate signing key in HSM
PKCS11_URI="pkcs11:id=%01;pin-value=123456"
# our code-signing certificate and all intermediate and CA certificates
CERT_SIGNING_CHAIN=~/certs/code-signing-chain.pem
# CA timestamp service
CA_TIMESTAMP=http://timestamp.sectigo.com

sign:	upx
	@if [ -e `which osslsigncode` ]; then \
		$(MKDIR_P) signed ;\
		for f in setup$(EXEEXT) setup-uncompressed${EXEEXT} ; do \
		    rm -f signed/$$f ;\
		    osslsigncode sign \
			-pkcs11module $(PKCSMODULE) -key $(PKCS11_URI) -nolegacy \
			-certs $(CERT_SIGNING_CHAIN) \
			-ts $(CA_TIMESTAMP) \
			-n 'Cygwin Setup' -i https://cygwin.com/ \
			-in upx/$$f -out signed/$$f ;\
		done \
	else \
		echo "osslsigncode not found, cannot sign setup$(EXEEXT)." ;\
	fi

release: sign
	@$(MKDIR_P) release
	cp stripped/setup.dbg release/setup-${VER}.${ARCH}.dbg
	cp signed//setup-uncompressed${EXEEXT} release/setup-${VER}-uncompressed.${ARCH}${EXEEXT}
	cp signed/setup${EXEEXT} release/setup-${VER}.${ARCH}${EXEEXT}

UPLOAD_HOST=cygwin.com
UPLOAD_PATH=/www/sourceware/htdocs/cygwin/setup
GPG_SIGN_KEYS="--enable-dsa2 --personal-digest-preferences=sha256 -u 1A698DE9E2E56300"

upload: release
	cd release ; scp setup-${VER}.$(ARCH).exe setup-${VER}.$(ARCH).dbg setup-${VER}-uncompressed.${ARCH}${EXEEXT} ${UPLOAD_HOST}:${UPLOAD_PATH}
	ssh ${UPLOAD_HOST} gpg --batch ${GPG_SIGN_KEYS} --detach-sign ${UPLOAD_PATH}/setup-${VER}.$(ARCH).exe
	ssh ${UPLOAD_HOST} gpg --batch ${GPG_SIGN_KEYS} --detach-sign ${UPLOAD_PATH}/setup-${VER}-uncompressed.$(ARCH).exe
	ssh ${UPLOAD_HOST} "cd ${UPLOAD_PATH} && sha512sum *.exe > sha512.sum"

clean-local:
	rm -rf stripped upx sign release
