# Copyright (c) 2000, Red Hat, Inc.
#
#     This program is free software; you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation; either version 2 of the License, or
#     (at your option) any later version.
#
#     A copy of the GNU General Public License can be found at
#     http://www.gnu.org/
#
# Written by Christopher Faylor <cgf@redhat.com>
# and Robert Collins <rbtcollins@hotmail.com>
#
# Makefile for Cygwin installer

AUTOMAKE_OPTIONS = foreign

AM_CFLAGS	= -mwindows -U_WIN32
AM_CXXFLAGS	= -fno-exceptions -fno-rtti -mwindows -U_WIN32

noinst_LIBRARIES = libbz2.a

libbz2_a_SOURCES = \
  	blocksort.c \
  	bzlib.c \
	compress.c \
	crctable.c \
	decompress.c \
	huffman.c \
	randtable.c \
	bzlib.h \
	bzlib_private.h
