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

#ifndef _IO_STREAM_RSYNC_H_
#define _IO_STREAM_RSYNC_H_

#include "io_stream.h"
#include "String++.h"

/* io_stream to an rsync server.
 * TODO: Implement :}.
 * TODO: deal with passing the basis in cleanly. For now, the basis is
 * assumed to be blank if the stream is opened via the generic interface,
 * and not if provided to the custom constructor...
 * The basis MUST support seek (0).
 */

class io_stream_rsync:public io_stream
{
public:
  static int exists (String const &);
  /* not currently supported 
  static int remove (String const &);
  static int mklink (String const &, String const &, io_stream_link_t);
  */
  io_stream_rsync (String const &,String const &, io_stream * = 0);
  virtual ~ io_stream_rsync ();
  /* read data (duh!) */
  virtual ssize_t read (void *buffer, size_t len);
  /* provide data to (double duh!) (NB: not currently supported) */
  virtual ssize_t write (const void *buffer, size_t len);
  /* read data without removing it from the class's internal buffer */
  virtual ssize_t peek (void *buffer, size_t len);
  virtual long tell ();
  /* Not currently supported */
  virtual int seek (long where, io_stream_seek_t whence);
  /* can't guess, oh well */
  virtual int error ();
  /* Not currently supported */
  virtual int set_mtime (int);
  /* not currently supported */
  virtual int get_mtime ()
  {
    return 0;
  };
  /* IMPLEMENT ME */
  virtual size_t get_size ();
  /* Not currently supported 
  static int move (String const &,String const &);
  */
private:
  /* always require parameters */
  io_stream_rsync ()
  {
  };
  io_stream *basis;
  int lasterr;
  String fname;
  String lmode;
};

#endif /* _IO_STREAM_RSYNC_H_ */
