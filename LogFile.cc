/*
 * Copyright (c) 2002, Robert Collins.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by Robert Collins <rbtcollins@hotmail.com>
 *
 */

/* Log to one or more files. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "LogFile.h"
#include "io_stream.h"
#include "win32.h"
#include "list.h"
#include "msg.h"
#include "resource.h"
#include <iostream>
#include <strstream>
#include <time.h>

/* private helper class */
class filedef
{
public:
  int level;
  String key;
  bool append;
};

/* another */
struct LogEnt
{
  LogEnt *next;
  enum log_level level;
  time_t when;
  String msg;
};

static LogEnt *first_logent = 0;
static LogEnt **next_logent = &first_logent;
static LogEnt *currEnt = 0;
int exit_msg = 0;

static list<filedef, String, String::casecompare> files;
static ostrstream *theStream;

LogFile::LogFile()
{
  theStream = new ostrstream;
  rdbuf (theStream->rdbuf());
}
LogFile::~LogFile(){}

void
LogFile::clearFiles ()
{
  while (files.number())
    {
      filedef *f = files.removebyindex(1);
      delete f;
    }
}

void
LogFile::setFile (int minlevel, String const &path, bool append)
{
  filedef *f = files.getbykey (path);
  if (!f)
    {
      f = new filedef;
      f->key = path;
      files.registerbyobject (*f);
    }
  f->level = minlevel;
  f->append = append;
}

void
LogFile::exit (int const exit_code)
{
  static int been_here = 0;
  if (been_here)
#ifndef _CYGWIN_
    ExitProcess (1);
#else
    exit (1);
#endif
  been_here = 1;
  
  if (exit_msg)
    note (NULL, exit_msg);
  
  log (LOG_TIMESTAMP) << "Ending cygwin install" << endLog;

  for (unsigned int i = 1; i <= files.number(); ++i)
    {
      filedef *f = files[i];
      log_save (f->level, f->key, f->append);
    }
#ifndef _CYGWIN_
  ExitProcess (exit_code);
#else
  exit (1);
#endif
}

void
LogFile::log_save (int babble, String const &filename, bool append)
{
  static int been_here = 0;
  if (been_here)
    return;
  been_here = 1;

  io_stream::mkpath_p (PATH_TO_FILE, String("file://") + filename);

  io_stream *f = io_stream::open(String("file://") + filename, append ? "at" : "wt");
  if (!f)
    {
      fatal (NULL, IDS_NOLOGFILE, filename.cstr_oneuse());
      return;
    }

  LogEnt *l;

  for (l = first_logent; l; l = l->next)
    {
      if (babble || !(l->level == LOG_BABBLE))
        {
          char *tstr = l->msg.cstr();
          f->write (tstr, strlen (tstr));
          if (tstr[strlen (tstr) - 1] != '\n')
            f->write ("\n", 1);
        }
    }

  delete f;
  been_here = 0;
}

ostream &
LogFile::operator() (log_level theLevel)
{
  if (theLevel < 1 || theLevel > 2)
    throw "barfoo";
  if (!theStream)
    theStream = new ostrstream;
  rdbuf (theStream->rdbuf());
  currEnt = new LogEnt;
  currEnt->next = 0;
  currEnt->level = theLevel;
  return *this;
}

void
LogFile::endEntry()
{
  if (!currEnt)
    {
      /* get a default LogEnt */
      currEnt = new LogEnt;
      currEnt->next = 0;
      currEnt->level = LOG_PLAIN;
    }
  *next_logent = currEnt;
  next_logent = &(currEnt->next);
  time (&(currEnt->when));
  if (currEnt->level == LOG_TIMESTAMP)
    {
      char b[100];
      struct tm *tm = localtime (&(currEnt->when));
      strftime (b, 1000, "%Y/%m/%d %H:%M:%S ", tm);
      currEnt->msg = b;
    }
  currEnt->msg += theStream->str();
  msg ("LOG: %d %s", currEnt->level, theStream->str());
  theStream->freeze(0);
  delete theStream;
  /* reset for next use */
  theStream = new ostrstream;
  rdbuf (theStream->rdbuf());
}
