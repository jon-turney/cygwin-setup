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

#ifndef _IO_STREAM_CYGFILE_H_
#define _IO_STREAM_CYGFILE_H_

#include "io_stream.h"
#include "String++.h"

/* io_stream on disk files using cygwin paths
 * and potentially understanding links in the future
 */

extern int cygmkdir_p (enum path_type_t isadir, String const &path);

class io_stream_cygfile:public io_stream
{
public:
  static int exists (String const &);
  static int remove (String const &);
  static int mklink (String const &, String const &, io_stream_link_t);
    io_stream_cygfile (String const &, String const &);
    virtual ~ io_stream_cygfile ();
  /* read data (duh!) */
  virtual ssize_t read (void *buffer, size_t len);
  /* provide data to (double duh!) */
  virtual ssize_t write (const void *buffer, size_t len);
  /* read data without removing it from the class's internal buffer */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual long tell ();
  virtual int seek (long where, io_stream_seek_t whence);
  /* can't guess, oh well */
  virtual int error ();
  virtual int set_mtime (int);
  /* not relevant yet */
  virtual int get_mtime ()
  {
    return 0;
  };
  virtual size_t get_size ();
  /* dummy for io_stream_file */
  virtual String next_file_name ()
  {
    return NULL;
  };
  static int move (String const &,String const &);
private:
  /* always require parameters */
  io_stream_cygfile ()
  {
  };
  FILE *fp;
  int lasterr;
  String fname;
  String lmode;
};

#endif /* _IO_STREAM_CYGFILE_H_ */
