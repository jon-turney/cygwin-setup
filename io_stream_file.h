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

#ifndef _IO_STREAM_FILE_H_
#define _IO_STREAM_FILE_H_


/* io_stream on disk files
 */

class io_stream_file:public io_stream
{
public:
  static int exists (const char *);
  static int remove (const char *);
  static int mklink (const char *, const char *, io_stream_link_t);
    io_stream_file (const char *, const char *);
  virtual ~io_stream_file ();
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
  virtual int get_mtime () {return 0;};
  /* dummy for io_stream_file */
  virtual const char *next_file_name ()
  {
    return NULL;
  };
private:
  /* always require parameters */
    io_stream_file ()
  {
  };
  FILE *fp;
  int lasterr;
  char *fname;
  char *lmode;
};

#endif /* _IO_STREAM_FILE_H_ */
