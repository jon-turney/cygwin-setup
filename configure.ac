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
dnl $Id$
dnl
dnl Autoconf configure script for Cygwin utilities.
dnl
dnl Process this file with autoconf to produce a configure script.

AC_INIT
AC_PREREQ(2.53)
AC_CONFIG_AUX_DIR(cfgaux)
AM_INIT_AUTOMAKE(setup, 0)
dnl AM_CONFIG_HEADER(include/autoconf.h)
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([Makefile.in])
AC_REVISION($Revision$)dnl

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

AC_MSG_CHECKING([Whether to enable rsync support])
AC_ARG_ENABLE(rsync,
	      AC_HELP_STRING([--enable-rsync],
			     [Compile in rsync support]),
	      ac_cv_enable_rsync=$enableval, ac_cv_enable_rsync=no)
AC_MSG_RESULT([$ac_cv_enable_rsync])
if test $ac_cv_enable_rsync = yes; then
  AM_CONDITIONAL(ENABLERSYNC, true)
  RSYNC="rsync"
  RSYNCLIB=rsync/librsync.la
  AC_CONFIG_SUBDIRS(rsync)
else
  AM_CONDITIONAL(ENABLERSYNC, false)
  RSYNC=
  RSYNCLIB=
fi
AC_SUBST(RSYNC)
AC_SUBST(RSYNCLIB)

AC_LANG_CPLUSPLUS
AC_PROG_CXX
AM_PROG_CC_C_O
AM_PROG_LEX
AC_PROG_YACC
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_LIBTOOL

dnl AC_CHECK_TOOL(AR, ar, ar)
dnl AC_SUBST(AR)
dnl AC_CHECK_TOOL(AS, as, as)
dnl AC_SUBST(AS)
dnl AC_CHECK_TOOL(RANLIB, ranlib, ranlib)
dnl AC_SUBST(RANLIB)
dnl AC_CHECK_TOOL(LD, ld, ld)
dnl AC_SUBST(LD)
dnl AC_CHECK_TOOL(DLLTOOL, dlltool, dlltool)
dnl AC_SUBST(DLLTOOL)
AC_CHECK_TOOL(WINDRES, windres, windres)
AC_SUBST(WINDRES)
dnl AC_CHECK_TOOL(OBJCOPY, objcopy, objcopy)
dnl AC_SUBST(OBJCOPY)

AC_CHECK_LIB(mingw32,main)

AC_CHECK_HEADERS(alloca.h \
		 errno.h \
		 string \
		 string.h )

AC_CONFIG_SUBDIRS(zlib)
AC_CONFIG_SUBDIRS(bz2lib)
AC_CONFIG_SUBDIRS(libgetopt++)

dnl add portability sources to inilint
case "$host_os" in
mingw32)
  AM_CONDITIONAL(MINGWTARGET, true)
  ;;
*)
  AM_CONDITIONAL(MINGWTARGET, false)
  ;;
esac

AC_CONFIG_FILES([Makefile tests/Makefile])
AC_OUTPUT
