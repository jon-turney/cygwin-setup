/*
 * Copyright (c) 2008, Dave Korn.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 *  This implements the ExtraKeysSetting class, which persists and reads
 * in (and saves) extra public DSA signing keys for the verification process.
 * It stores them all in a contiguous memory buffer.  Each is one line of
 * ASCII text terminated by LF.  THERE IS NO NUL-TERMINATION HERE, TAKE CARE!
 * The buffer is sized to the exact size of the content including the terminating
 * LF of the last entry.  There is no zero after it.  After reading the file,
 * any partial last line is truncated.
 *
 * Written by Dave Korn <dave.korn.cygwin@gmail.com>
 *
 */

#include <stdlib.h>
#include <string.h>
#include "UserSettings.h"
#include "io_stream.h"
#include "KeysSetting.h"

ExtraKeysSetting *ExtraKeysSetting::global;

ExtraKeysSetting::ExtraKeysSetting ():
  keybuffer (NULL), bufsize (0), numkeys (0)
{
  global = this;
  const char *p = UserSettings::instance().get ("extrakeys");
  if (p)
    {
      bufsize = strlen (p) + 1;	// Include final NUL.
      keybuffer = strdup (p);
      // Replace final NUL by LF.
      keybuffer[bufsize - 1] = 0x0a;
      // Calling count_keys gets the count but also sizes the buffer
      // correctly, discarding any trailing non-LF-terminated data.
      bufsize = count_keys ();
    }
}

ExtraKeysSetting::~ExtraKeysSetting ()
{
  if (keybuffer)
    {
      // Replace final LF by NUL.
      keybuffer[bufsize - 1] = '\0';
      UserSettings::instance().set ("extrakeys", keybuffer);
    }
}

void
ExtraKeysSetting::flush (void)
{
  if (bufsize)
    delete [] keybuffer;
  keybuffer = 0;
  bufsize = 0;
  numkeys = 0;
}

void
ExtraKeysSetting::realloc (size_t newbufsize)
{
  char *newbuf = new char[newbufsize];
  if (bufsize)
    {
      memcpy (newbuf, keybuffer, newbufsize < bufsize ? newbufsize : bufsize);
      delete [] keybuffer;
    }
  keybuffer = newbuf;
  bufsize = newbufsize;
}

size_t
ExtraKeysSetting::count_keys (void)
{
  size_t offs = 0, size = 0;
  numkeys = 0;
  while (offs < bufsize)
    if (keybuffer[offs++] == 0x0a)
      {
        size = offs;
	++numkeys;
      }
  return size;
}

size_t
ExtraKeysSetting::num_keys (void)
{
  return numkeys;
}

const char *
ExtraKeysSetting::get_key (size_t num, size_t *size)
{
  if (!numkeys || (num >= numkeys))
    return NULL;

  const char *ptr = keybuffer;
  while (num--)
    while (*ptr++ != 0x0a);

  // Count its size if requested.
  if (size)
    {
      const char *ptr2 = ptr;
      while (num--)
        if (*ptr2 != 0x0a)
	  ++ptr2;
	else
	  break;
      *size = ptr2 - ptr;
    }
  return ptr;
}

void
ExtraKeysSetting::add_unique_key (const char *key)
{
  size_t osize = bufsize;
  realloc (bufsize + strlen (key) + 1);
  strcpy (keybuffer + osize, key);
  keybuffer[bufsize - 1] = 0x0a;
  ++numkeys;
}

void
ExtraKeysSetting::add_key (const char *key)
{
  /* Only add key if we don't already have it. */
  const char *ptr = keybuffer;
  size_t remain = bufsize;
  size_t keylen = strlen (key);

  while (remain >= keylen)
    {
      if (memcmp (ptr, key, keylen) == 0)
        return;

      while (remain--)
	if (*ptr++ == 0x0a)
	  break;
    }
  add_unique_key (key);
}
