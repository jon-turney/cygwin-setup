/*
 * Copyright (c) 2018, Cygwin
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 */

#ifndef SETUP_COMPRESS_ZSTD_H
#define SETUP_COMPRESS_ZSTD_H

#include "compress.h"
#include <zstd.h>

class compress_zstd:public compress
{
public:
  compress_zstd (io_stream *); /* decompress (read) only */
  virtual ssize_t read (void *buffer, size_t len);
  virtual ssize_t write (const void *buffer, size_t len); /* not implemented */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual long tell (); /* not implemented */
  virtual int seek (long where, io_stream_seek_t whence);
  virtual int error ();
  virtual const char *next_file_name () { return NULL; };
  virtual int set_mtime (time_t);
  virtual time_t get_mtime ();
  virtual mode_t get_mode ();
  virtual size_t get_size () {return 0;};
  virtual ~compress_zstd ();
  static bool is_zstd (void *buffer, size_t len);
  virtual void release_original(); /* give up ownership of original io_stream */

private:
  compress_zstd () {};

  io_stream *original;
  bool owns_original;
  int lasterr;
  void create ();
  void destroy ();

  struct private_data {
    ZSTD_DStream    *stream;
    ZSTD_outBuffer   out_block;
    size_t           out_bsz;
    size_t           out_pos;
    uint64_t         total_out;
    char             eof; /* True = found end of compressed data. */
    ZSTD_inBuffer    in_block;
    size_t           in_bsz;
    size_t           in_pos;
    uint64_t         total_in;
    size_t           in_processed;
    size_t           out_processed;
  };

  struct private_data *state;
};

#endif /* SETUP_COMPRESS_ZSTD_H */
