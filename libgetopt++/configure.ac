dnl
dnl  Configuration input file for GetOpt++
dnl
dnl  Robert Collins, rbtcollins@hotmail.com
dnl
dnl  $Id$
dnl
dnl
dnl
AC_INIT(src/GetOption.cc)
AC_PREREQ(2.52)
AC_CONFIG_AUX_DIR(cfgaux)
AM_INIT_AUTOMAKE(GetOpt++, 0.0.1-DEVEL)
AM_CONFIG_HEADER(include/autoconf.h)
AC_REVISION($Revision$)dnl
AC_PREFIX_DEFAULT(/usr/local)
AM_MAINTAINER_MODE

dnl Set default LDFLAGS
if test -z "$LDFLAGS"; then
        LDFLAGS="-g"
fi

PRESET_CFLAGS="$CFLAGS"

dnl Check for GNU cc
AC_LANG_CPLUSPLUS
dnl AC_API_WIN32
AC_PROG_CXX
AM_PROG_CC_C_O
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_PROG_LIBTOOL

CRYPTLIB=''
REGEXLIB=''	# -lregex
LIBREGEX=''	# libregex.a

dnl find out the exe extension for this platform. If it's not empty, use it for CGI's as well.
AC_EXEEXT
AC_OBJEXT

AC_CHECK_HEADERS( \
	unistd.h \
	string   \
	string.h
)

AC_C_CONST
AC_C_BIGENDIAN

AC_TYPE_SIZE_T 
AC_CHECK_TYPE(ssize_t, int)
AC_TYPE_PID_T
AC_TYPE_MODE_T

dnl Check for special functions
AC_FUNC_ALLOCA

dnl Check for needed libraries

AC_CHECK_LIB(regex, main, [REGEXLIB="-lregex"])
AC_CHECK_LIB(m, main)

dnl Check for libcrypt
dnl We use crypt(3) which may be in libc, or in libcrypt (eg FreeBSD)
AC_CHECK_LIB(crypt, crypt, [CRYPTLIB="-lcrypt"])
AC_SUBST(CRYPTLIB)

dnl Check for pthreads
dnl We use pthreads when doing ASYNC I/O
AC_CHECK_LIB(pthread, main, [PTHREADLIB="-lpthread"])
AC_SUBST(PTHREADLIB)

dnl Check for librt
dnl We use AIO in the coss store
AC_CHECK_LIB(rt, aio_read, [AIOLIB="-lrt"])
AC_SUBST(AIOLIB)

dnl Check for library functions
dnl AC_CHECK_FUNCS(\
dnl )


AC_OUTPUT([\
	Makefile
])
