/*
 * Copyright (c) 2001, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins  <rbtcollins@hotmail.com>
 *
 */

#ifndef SETUP_COMPRESS_GZ_H
#define SETUP_COMPRESS_GZ_H

#include "compress.h"
#include <zlib.h>

class compress_gz:public compress
{
public:
  /* assumes decompression */
  compress_gz (io_stream *);
  /* the mode allows control, but this implementation only does compression */
  compress_gz (io_stream *, const char *);
  /* read data (duh!) */
  virtual ssize_t read (void *buffer, size_t len);
  /* provide data to (double duh!) */
  virtual ssize_t write (const void *buffer, size_t len);
  /* read data without removing it from the class's internal buffer */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual off_t tell ();
  virtual off_t seek (off_t where, io_stream_seek_t whence);
  /* try guessing this one */
  virtual int error ();
  /* Find out the next stream name -
   * ie for foo.tar.gz, at offset 0, next_file_name = foo.tar
   * for foobar that is an compress, next_file_name is the next
   * extractable filename.
   */
  virtual const char *next_file_name ()
  {
    return NULL;
  };
  virtual int set_mtime (time_t);
  virtual time_t get_mtime ();
  virtual mode_t get_mode ();
  /* Use seek EOF, then tell (). get_size won't do this incase you are sucking down
   * over a WAN :} */
  virtual size_t get_size () {return 0;};
  virtual void release_original (); /* give up ownership of original io_stream */
  /* if you are still needing these hints... give up now! */
  virtual ~ compress_gz ();
private:
    compress_gz ()
  {
  };
  char peekbuf[512];
  size_t peeklen;
  void construct ();
  void check_header ();
  int get_byte ();
  unsigned long getLong ();
  void putLong (unsigned long);
  void destroy ();
  int do_flush (int);
  io_stream *original;
  bool owns_original;
  const char *openmode;
  /* from zlib */
  z_stream stream;
  int z_err;			/* error code for last stream operation */
  int z_eof;			/* set if end of input file */
  unsigned char *inbuf;		/* input buffer */
  unsigned char *outbuf;	/* output buffer */
  uLong crc;			/* crc32 of uncompressed data */
  char *msg;			/* error message */
  int transparent;		/* 1 if input file is not a .gz file */
  char mode;			/* 'w' or 'r' */
  off_t startpos;		/* start of compressed data in file (header skipped) */
};

#endif /* SETUP_COMPRESS_GZ_H */
