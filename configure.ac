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

dnl Autoconf configure script for Cygwin utilities.
dnl
dnl Process this file with autoconf to produce a configure script.

AC_INIT
AC_PREREQ(2.53)
AC_CONFIG_AUX_DIR(cfgaux)
AC_CONFIG_SRCDIR([Makefile.in])
AC_REVISION($Revision$)dnl

AC_PROG_CXX
AC_PROG_CC_C_O
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_INSTALL

AC_EXEEXT

AC_CHECK_TOOL(AR, ar, ar)
AC_SUBST(AR)
AC_CHECK_TOOL(AS, as, as)
AC_SUBST(AS)
AC_CHECK_TOOL(RANLIB, ranlib, ranlib)
AC_SUBST(RANLIB)
AC_CHECK_TOOL(LD, ld, ld)
AC_SUBST(LD)
AC_CHECK_TOOL(DLLTOOL, dlltool, dlltool)
AC_SUBST(DLLTOOL)
AC_CHECK_TOOL(WINDRES, windres, windres)
AC_SUBST(WINDRES)
AC_CHECK_TOOL(OBJCOPY, objcopy, objcopy)
AC_SUBST(OBJCOPY)

AC_CONFIG_SUBDIRS(zlib)
AC_CONFIG_SUBDIRS(bz2lib)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
