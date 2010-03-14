/*
 * Copyright (c) 2010, Charles Wilson
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

#ifndef SETUP_COMPRESS_XZ_H
#define SETUP_COMPRESS_XZ_H

/* this is the parent class for all compress IO operations. 
 */

#include "compress.h"
#include <lzma.h>

class compress_xz:public compress
{
public:
  compress_xz (io_stream *); /* decompress (read) only */
  virtual ssize_t read (void *buffer, size_t len);
  virtual ssize_t write (const void *buffer, size_t len); /* not implemented */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual long tell (); /* not implemented */
  virtual int seek (long where, io_stream_seek_t whence); /* not implemented */
  virtual int error ();
  virtual const char *next_file_name () { return NULL; };
  virtual int set_mtime (time_t);
  virtual time_t get_mtime ();
  virtual mode_t get_mode ();
  virtual size_t get_size () {return 0;};
  virtual ~compress_xz ();
  virtual void init_decoder (void);
  static bool is_xz_or_lzma (void *buffer, size_t len);
  static int  bid_xz   (void *buffer, size_t len);
  static int  bid_lzma (void *buffer, size_t len);
  virtual void release_original(); /* give up ownership of original io_stream */

private:
  compress_xz () {};

  io_stream *original;
  bool owns_original;
  char peekbuf[512];
  size_t peeklen;
  int lasterr;
  void destroy ();

  struct private_data {
    lzma_stream      stream;
    unsigned char   *out_block;
    size_t           out_block_size;
    uint64_t         total_out;
    char             eof; /* True = found end of compressed data. */
    unsigned char   *in_block;
    size_t           in_block_size;
    uint64_t         total_in;
    unsigned char   *out_p;
    size_t           in_pos;
    size_t           out_pos;
    size_t           in_size;
    size_t           in_processed;
    size_t           out_processed;
  };

  typedef enum {
    COMPRESSION_UNKNOWN = -1,
    COMPRESSION_XZ,
    COMPRESSION_LZMA
  } compression_type_t;

  compression_type_t compression_type;

  static const size_t out_block_size = 64 * 1024;
  static const size_t in_block_size = 64 * 1024;
  struct private_data *state;
};

#endif /* SETUP_COMPRESS_XZ_H */
