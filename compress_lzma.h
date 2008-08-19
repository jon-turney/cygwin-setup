/*
 * Copyright (c) 2008, Charles Wilson
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Charles Wilson <cygwin@cygwin.com>
 *
 */

#ifndef SETUP_COMPRESS_LZMA_H
#define SETUP_COMPRESS_LZMA_H

/* this is the parent class for all compress IO operations. 
 */

#include "compress.h"
#include "lzma-sdk/LzmaDec.h"

class compress_lzma:public compress
{
public:
  compress_lzma (io_stream *); /* decompress (read) only */
  virtual ssize_t read (void *buffer, size_t len);
  virtual ssize_t write (const void *buffer, size_t len); /* not implemented */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual long tell (); /* not implemented */
  virtual int seek (long where, io_stream_seek_t whence); /* not implemented */
  virtual int error ();
  virtual const char *next_file_name () { return NULL; };
  virtual int set_mtime_and_mode (time_t, mode_t);
  virtual time_t get_mtime ();
  virtual mode_t get_mode ();
  virtual size_t get_size () {return 0;};
  virtual ~compress_lzma ();
  static bool is_lzma(void *buffer, size_t len);
  virtual void release_original(); /* give up ownership of original io_stream */

private:
  compress_lzma () {};

  io_stream *original;
  bool owns_original;
  char peekbuf[512];
  size_t peeklen;
  int lasterr;
  bool initializedOk;
  void check_header ();
  void destroy ();

  /* from lzma */
  static const size_t IN_BUF_SIZE  = (1 << 16);
  static const size_t OUT_BUF_SIZE = (1 << 16);
  unsigned char header[LZMA_PROPS_SIZE + 8];
  UInt64 unpackSize;
  bool thereIsSize;  /* true means header specifies uncompressed size */
  CLzmaDec state;
  unsigned char * inbuf;
  unsigned char * outbuf;
  unsigned char * outp;
  size_t inPos;
  size_t inSize;
  size_t outPos;
  SizeT inProcessed;
  SizeT outProcessed;
};

#endif /* SETUP_COMPRESS_LZMA_H */
