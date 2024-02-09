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
AC_PREREQ(2.69)
AC_CONFIG_AUX_DIR([cfgaux])
AM_INIT_AUTOMAKE([1.12 subdir-objects foreign no-define -Wall -Wno-portability])
dnl AM_CONFIG_HEADER(include/autoconf.h)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES(yes)])
AC_CONFIG_SRCDIR([Makefile.in])

AC_LANG([C++])
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_LEX([noyywrap])
AC_PROG_YACC
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
LT_INIT

AC_CHECK_TOOL(WINDRES, windres, windres)
AC_CHECK_TOOL(OBJCOPY, objcopy, objcopy)

dnl dependencies we can check for using pkgconfig
PKG_CHECK_MODULES(ZLIB, [zlib])
PKG_CHECK_MODULES(LZMA, [liblzma])
PKG_CHECK_MODULES(ZSTD, [libzstd])
PKG_CHECK_MODULES(LIBSOLV, [libsolv])

dnl dependencies we need to check for by hand
export LIBGCRYPT_CONFIG=$($CC --print-sysroot)/mingw/bin/libgcrypt-config
AM_PATH_LIBGCRYPT

save_LIBS=$LIBS
LIBS="$LIBS -lbz2"
AC_MSG_CHECKING([for bzip2])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <bzlib.h>]], [[const char *version = BZ2_bzlibVersion ();]])],[HAVE_BZ2=yes],[HAVE_BZ2=no])
AC_MSG_RESULT($HAVE_BZ2)
LIBS=$save_LIBS
if test "x$HAVE_BZ2" = "xno"; then
    AC_MSG_ERROR([bzip2 not found])
  else
    BZ2_LIBS="-lbz2"
fi
AC_SUBST(BZ2_LIBS)

dnl configure in libgetopt++
prefix=`pwd`/inst; mkdir -p "$prefix"
exec_prefix=$prefix
ac_configure_args="$ac_configure_args --disable-shared"
AC_CONFIG_SUBDIRS(libgetopt++)

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
AM_CONDITIONAL(ARCH_X86, [test "x$ARCH" = "xx86"])

dnl check exception personality
AC_MSG_CHECKING([compiler exception personality])
AC_LANG([C++])
AC_COMPILE_IFELSE(
  [AC_LANG_SOURCE([
#ifdef _X86_
#ifndef __USING_SJLJ_EXCEPTIONS__
#error not using sjlj exceptions
#endif
#endif
  ])],
  [AC_MSG_RESULT([ok])],
  [AC_MSG_FAILURE([Must not be built with a compiler which uses dwarf2 exception handling])])

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT
