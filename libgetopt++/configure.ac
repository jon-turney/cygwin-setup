dnl
dnl  Configuration input file for GetOpt++
dnl
dnl  Robert Collins, rbtcollins@hotmail.com
dnl
dnl
AC_INIT(src/GetOption.cc, 0.0.2-DEVEL)
AC_PREREQ(2.69)
AC_CONFIG_AUX_DIR(cfgaux)
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(include/autoconf.h)
AC_PREFIX_DEFAULT(/usr/local)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES(yes)])

AC_LANG([C++])
AC_PROG_CXX
AM_PROG_CC_C_O
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
LT_INIT

AC_CHECK_HEADERS( \
	unistd.h \
	string   \
	string.h
)

AC_CHECK_HEADER(getopt.h,
[
AM_CONDITIONAL(INTERNAL_GETOPT, false)
],[
AM_CONDITIONAL(INTERNAL_GETOPT, true)
])


AC_C_CONST
AC_C_BIGENDIAN

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
