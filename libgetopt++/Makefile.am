## Process this file with automake to produce Makefile.in
#
# $Id$
#

AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects 1.5
##DIST_SUBDIRS	= src
##SUBDIRS		= src

DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]*

INCLUDES = -I$(top_srcdir)/include

getoptincludedir = $(includedir)/getopt++

lib_LTLIBRARIES = libgetopt++.la
check_PROGRAMS = testoption

EXTRA_libgetopt___la_SOURCES = src/getopt.c

if INTERNAL_GETOPT
internal_getopt=src/getopt.c
else
internal_getopt=
endif

libgetopt___la_SOURCES = src/GetOption.cc src/Option.cc src/BoolOption.cc \
	src/OptionSet.cc \
	src/StringOption.cc \
	$(internal_getopt) \
	include/getopt.h \
	include/cdefs.h

libgetopt___la_LDFLAGS = -version-info 0:0:0

getoptinclude_HEADERS = include/getopt++/Option.h \
  include/getopt++/GetOption.h \
  include/getopt++/BoolOption.h include/getopt++/OptionSet.h \
  include/getopt++/StringOption.h

testoption_SOURCES = tests/testoption.cc
testoption_LDADD = libgetopt++.la
