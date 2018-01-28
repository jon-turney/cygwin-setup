dnl Copyright (c) 2000, Red Hat, Inc.
dnl
dnl     This program is free software; you can redistribute it and/or modify
dnl     it under the terms of the GNU General Public License as published by
dnl     the Free Software Foundation; either version 2 of the License, or
dnl     (at your option) any later version.
dnl
dnl     A copy of the GNU General Public License can be found at
dnl     http://www.gnu.org/
dnl
dnl Written by Christopher Faylor <cgf@redhat.com>
dnl and Robert Collins  <rbtcollins@hotmail.com>
dnl
dnl Autoconf configure script for Cygwin utilities.
dnl
dnl Process this file with autoconf to produce a configure script.

AC_INIT([setup], [0], [cygwin-apps@cygwin.com])
AC_PREREQ(2.60)
AC_CONFIG_AUX_DIR([cfgaux])
AM_INIT_AUTOMAKE([1.12 subdir-objects foreign no-define -Wall -Wno-portability])
dnl AM_CONFIG_HEADER(include/autoconf.h)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES(yes)])
AC_CONFIG_SRCDIR([Makefile.in])

AC_MSG_CHECKING([Whether to build inilint])
AC_ARG_ENABLE(inilint,
	    AC_HELP_STRING([--enable-inilint],
			   [Build the inilint tool]),
	    ac_cv_enable_inilint=$enableval, ac_cv_enable_inilint=no)
AC_MSG_RESULT([$ac_cv_enable_inilint])
if test $ac_cv_enable_inilint = yes; then
  INILINT="inilint\$(EXEEXT)"
else
  INILINT=
fi
AC_SUBST(INILINT)

AC_LANG_CPLUSPLUS
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_LEX
AC_PROG_YACC
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_LIBTOOL

AC_CHECK_TOOL(WINDRES, windres, windres)
AC_CHECK_TOOL(OBJCOPY, objcopy, objcopy)

AC_CHECK_HEADERS(alloca.h \
		 errno.h \
		 string \
		 string.h )

AC_CHECK_HEADER(zlib.h, , missing_deps="$missing_deps zlib")
AC_CHECK_HEADER(bzlib.h, , missing_deps="$missing_deps libbz2")
AC_CHECK_HEADER(lzma.h, , missing_deps="$missing_deps liblzma")
AC_CHECK_HEADER(gcrypt.h, , missing_deps="$missing_deps libgcrypt")
AC_CHECK_HEADER(solv/pool.h, , missing_deps="$missing_deps libsolv")
AC_CHECK_HEADER(regex.h, , missing_deps="$missing_deps libregex")

if test -n "$missing_deps"; then
	AC_MSG_ERROR([missing prerequisites: $missing_deps])
fi

prefix=`pwd`/inst; mkdir -p "$prefix"
exec_prefix=$prefix
ac_configure_args="$ac_configure_args --disable-shared"
AC_CONFIG_SUBDIRS(libgetopt++)

dnl add portability sources to inilint
case "$host" in
i?86-*-mingw32)
  SETUP="setup"
  ARCH="x86"
  ;;
x86_64-*-mingw32)
  SETUP="setup"
  ARCH="x86_64"
  ;;
*)
  AC_MSG_ERROR([Cygwin Setup can only be built for Win32 or Win64 hosts])
  ;;
esac
AC_SUBST(SETUP)
AC_SUBST(ARCH)

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT
