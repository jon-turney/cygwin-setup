/*
 * Copyright (c) 2001, 2002, Robert Collins.
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

/* this is the parent class for all IO operations. It's flexable enough to be cover for
 * HTTP access, local file access, and files being extracted from archives.
 * It also encapsulates the idea of an archive, and all non-archives become the special 
 * case.
 */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "LogSingleton.h"

#include "io_stream.h"
#include "String++.h"
#include "list.h"
#include <stdexcept>
#include "IOStreamProvider.h"

static list <IOStreamProvider const, String const, String::casecompare> *providers;
static size_t longestPrefix = 0;
static int inited = 0;
  
void
io_stream::registerProvider (IOStreamProvider &theProvider,
			     String const &urlPrefix)
{
  if (!inited)
    {
      providers = new list <IOStreamProvider const, String const, 
        String::casecompare>;
      inited = true;
    }
  theProvider.key = urlPrefix;
  IOStreamProvider const &testProvider = providers->registerbyobject (theProvider);
  if (&testProvider != &theProvider)
    throw new invalid_argument ("urlPrefix already registered!");
  if (urlPrefix.size() > longestPrefix)
    longestPrefix = urlPrefix.size();
}

static IOStreamProvider const *
findProvider (String const &path)
{
  if (path.size() < longestPrefix)
    return NULL;
  for (unsigned int i = 1; i <= providers->number(); ++i)
    {
      IOStreamProvider const *p = (*providers)[i];
      if (!path.casecompare (p->key, p->key.size()))
       	return p;
    }
  return NULL;
}

/* Static members */
io_stream *
io_stream::factory (io_stream * parent)
{
  /* something like,  
   * if !next_file_name 
   *   return NULL
   * switch (magic_id(peek (parent), max_magic_length))
   * case io_stream * foo = new tar
   * case io_stream * foo = new bz2
   * return foo
   */
  log (LOG_TIMESTAMP) <<  "io_stream::factory has been called" << endLog;
  return NULL;
}

io_stream *
io_stream::open (String const &name, String const &mode)
{
  IOStreamProvider const *p = findProvider (name);
  if (!p)
    throw new invalid_argument ("URL Scheme not registered!");
  io_stream *rv = p->open (&name.cstr_oneuse()[p->key.size()], mode);
  if (!rv->error ())
    return rv;
  delete rv;
  return NULL;
}

int
io_stream::mkpath_p (path_type_t isadir, String const &name)
{
  IOStreamProvider const *p = findProvider (name);
  if (!p)
    throw new invalid_argument ("URL Scheme not registered!");
  return p->mkdir_p (isadir, &name.cstr_oneuse()[p->key.size()]);
}

/* remove a file or directory. */
int
io_stream::remove (String const &name)
{
  IOStreamProvider const *p = findProvider (name);
  if (!p)
    throw new invalid_argument ("URL Scheme not registered!");
  return p->remove (&name.cstr_oneuse()[p->key.size()]);
}

int
io_stream::mklink (String const &from, String const &to,
		   io_stream_link_t linktype)
{
  log (LOG_BABBLE) << "io_stream::mklink (" << from << "->" << to << ")"
    << endLog;
  IOStreamProvider const *fromp = findProvider (from);
  IOStreamProvider const *top = findProvider (to);
  if (!fromp || !top)
    throw new invalid_argument ("URL Scheme not registered!");
  if (fromp != top)
    throw new invalid_argument ("Attempt to link across url providers.");
  return fromp->mklink (&from.cstr_oneuse()[fromp->key.size()], 
    			&to.cstr_oneuse()[top->key.size()], linktype);
}

int
io_stream::move_copy (String const &from, String const &to)
{
  /* parameters are ok - checked before calling us, and we are private */
  io_stream *in = io_stream::open (to, "wb");
  io_stream *out = io_stream::open (from, "rb");
  if (io_stream::copy (in, out))
    {
      log (LOG_TIMESTAMP) << "Failed copy of " << from << " to " << to
	<< endLog;
      delete out;
      io_stream::remove (to);
      delete in;
      return 1;
    }
  /* TODO:
     out->set_mtime (in->get_mtime ());
   */
  delete in;
  delete out;
  io_stream::remove (from);
  return 0;
}

ssize_t io_stream::copy (io_stream * in, io_stream * out)
{
  if (!in || !out)
    return -1;
  char
    buffer[16384];
  ssize_t
    countin,
    countout;
  while ((countin = in->read (buffer, 16384)) > 0)
    {
      countout = out->write (buffer, countin);
      if (countout != countin)
	{
	  log (LOG_TIMESTAMP) << "io_stream::copy failed to write "
	    << countin << " bytes" << endLog;
	  return countout ? countout : -1;
	}
    }
  // XXX FIXME: What if countin < 0? return an error

  /* TODO:
     out->set_mtime (in->get_mtime ());
   */
  return 0;
}

int
io_stream::move (String const &from, String const &to)
{
  IOStreamProvider const *fromp = findProvider (from);
  IOStreamProvider const *top = findProvider (to);
  if (!fromp || !top)
    throw new invalid_argument ("URL Scheme not registered!");
  if (fromp != top)
    return io_stream::move_copy (from, to);
  return fromp->move (&from.cstr_oneuse()[fromp->key.size()],
  		      &to.cstr_oneuse()[top->key.size()]);
}

char *
io_stream::gets (char *buffer, size_t length)
{
  char *pos = buffer;
  size_t count = 0;
  while (count + 1 < length && read (pos, 1) == 1)
    {
      count++;
      pos++;
      if (*(pos - 1) == '\n')
	{
	  --pos; /* end of line, remove from buffer */
	  if (pos > buffer && *(pos - 1) == '\r')
	    --pos;
	  break;
	}
    }
  if (count == 0 || error ())
    /* EOF when no chars found, or an error */
    return NULL;
  *pos = '\0';
  return buffer;
}

int
io_stream::exists (String const &name)
{
  IOStreamProvider const *p = findProvider (name);
  if (!p)
    throw new invalid_argument ("URL Scheme not registered!");
  return p->exists (&name.cstr_oneuse()[p->key.size()]);
}

/* virtual members */

io_stream::~io_stream ()
{
  if (!destroyed)
    log (LOG_TIMESTAMP) << "io_stream::~io_stream: It looks like a class "
      << "hasn't overriden the destructor!" << endLog;
  return;
}
