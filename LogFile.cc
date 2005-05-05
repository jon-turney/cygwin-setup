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
#include "msg.h"
#include "resource.h"
#include <iostream>
#include <sstream>
#include <set>
#include <time.h>
#include <string>
#include <stdexcept>
#include "AntiVirus.h"
#include "filemanip.h"

using namespace std;

/* private helper class */
class filedef
{
public:
  int level;
  String key;
  bool append;
  filedef (String const &_path) : key (_path) {}
  bool operator == (filedef const &rhs) const
    {
      return key.casecompare (rhs.key) == 0;
    }
  bool operator < (filedef const &rhs) const
    {
      return key.casecompare (rhs.key) < 0;
    }
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

typedef set<filedef> FileSet;
static FileSet files;
static stringbuf *theStream;

LogFile *
LogFile::createLogFile()
{
    theStream = new std::stringbuf;
    return new LogFile(theStream);
}

LogFile::LogFile(std::stringbuf *aStream) : LogSingleton (aStream) 
{
}
LogFile::~LogFile(){}

void
LogFile::clearFiles ()
{
  files.clear ();
}

void
LogFile::setFile (int minlevel, String const &path, bool append)
{
  FileSet::iterator f = files.find (filedef(path));
  if (f != files.end ())
    files.erase (f);
  
  filedef t (path);
  t.level = minlevel;
  t.append = append;
  files.insert (t);
}

String
LogFile::getFileName (int level) const
{
  for (FileSet::iterator i = files.begin();
       i != files.end(); ++i)
    {
      if (i->level == level)
        return i->key;
    }
  return "<no log was in use>";
}

void
LogFile::exit (int const exit_code)
{
  AntiVirus::AtExit();
  static int been_here = 0;
  if (been_here)
    ::exit (exit_code);
  been_here = 1;
  
  if (exit_msg)
    note (NULL, exit_msg, backslash(getFileName(LOG_BABBLE)).c_str());
  
  log (LOG_TIMESTAMP) << "Ending cygwin install" << endLog;

  for (FileSet::iterator i = files.begin();
       i != files.end(); ++i)
    {
      log_save (i->level, i->key, i->append);
    }
  // TODO: remove this when the ::exit issue is tidied up.
  ::exit (exit_code);
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
      fatal (NULL, IDS_NOLOGFILE, filename.c_str());
      return;
    }

  LogEnt *l;

  for (l = first_logent; l; l = l->next)
    {
      if (babble || !(l->level == LOG_BABBLE))
        {
          const char *tstr = l->msg.c_str();
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
    throw new invalid_argument("Invalid log_level");
  if (!theStream)
    theStream = new std::stringbuf;
  rdbuf (theStream);
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
  /* What follows is a hack to get around an (apparent) bug in libg++-3 with
   * non-0 memory on alloc
   */
  currEnt->msg += theStream->str();
    // OLD code (libg++3) string(theStream->str()).substr(0,theStream->pcount()).c_str();
  msg ("LOG: %d %s", currEnt->level, theStream->str().c_str());
       //string(theStream->str()).substr(0,theStream->rdbuf()->pcount()).c_str());
  // theStream->freeze(0);
  delete theStream;
  /* reset for next use */
  theStream = new std::stringbuf;
  rdbuf (theStream);
  init (theStream);
}
