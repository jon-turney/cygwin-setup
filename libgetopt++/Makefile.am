## Process this file with automake to produce Makefile.in
#
# $Id$
#

AUTOMAKE_OPTIONS = dist-bzip2 subdir-objects VERSION=1.5
##DIST_SUBDIRS	= src
##SUBDIRS		= src

DISTCLEANFILES = include/stamp-h include/stamp-h[0-9]*

AM_CXXFLAGS = -I$(top_srcdir)/include

getoptincludedir = $(includedir)/getopt++

lib_LTLIBRARIES = libgetopt++.la
check_PROGRAMS = testoption

libgetopt___la_SOURCES = src/GetOption.cc src/Option.cc src/BoolOption.cc
libgetopt___la_LDFLAGS = -lstdc++ -version-info 0:0:0

getoptinclude_HEADERS = include/Option.h include/GetOption.h \
	include/BoolOption.h

testoption_SOURCES = tests/testoption.cc
testoption_LDADD = libgetopt++.la
