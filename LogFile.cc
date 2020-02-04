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

#include <stdlib.h>
#include "LogFile.h"
#include "io_stream.h"
#include "win32.h"
#include "msg.h"
#include "dialog.h"
#include "resource.h"
#include <iostream>
#include <sstream>
#include <set>
#include <time.h>
#include <string>
#include <stdexcept>
#include "filemanip.h"
#include "String++.h"
#include "getopt++/BoolOption.h"

static BoolOption VerboseOutput (false, 'v', "verbose", IDS_HELPTEXT_VERBOSE);

/* private helper class */
class filedef
{
public:
  int level;
  std::string key;
  bool append;
  filedef (const std::string& _path) : key (_path) {}
  bool operator == (filedef const &rhs) const
    {
      return casecompare(key, rhs.key) == 0;
    }
  bool operator < (filedef const &rhs) const
    {
      return casecompare(key, rhs.key) < 0;
    }
};

/* another */
struct LogEnt
{
  LogEnt *next;
  enum log_level level;
  time_t when;
  std::string msg;
};

static LogEnt *first_logent = 0;
static LogEnt **next_logent = &first_logent;
static LogEnt *currEnt = 0;

int LogFile::exit_msg = 0;

typedef std::set<filedef> FileSet;
static FileSet files;
static std::stringbuf *theStream;

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
LogFile::setFile (int minlevel, const std::string& path, bool append)
{
  FileSet::iterator f = files.find (filedef(path));
  if (f != files.end ())
    files.erase (f);
  
  filedef t (path);
  t.level = minlevel;
  t.append = append;
  files.insert (t);
}

std::string
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
LogFile::atexit(void (*func)(void))
{
  exit_fns.push_back(func);
}

void
LogFile::exit (int exit_code, bool show_end_install_msg)
{
  /* Execute any functions we want to run at exit (we don't use stdlib atexit()
     because we want to allow them to potentially write to the log) */
  for (auto i = exit_fns.rbegin(); i != exit_fns.rend(); ++i)
      (*i)();

  static int been_here = 0;
  if (been_here)
    ::exit (exit_code);
  been_here = 1;

  if (exit_msg)
    {
      std::wstring fmt = LoadStringWEx(exit_msg, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));
      std::wstring buf = format(fmt, backslash(getFileName(LOG_BABBLE)).c_str());
      Log (LOG_PLAIN) << "note: " << wstring_to_string(buf) << endLog;
    }

  /* Skip the log messages when just printing the help/version output, and when
     we're self-elevating. */
  if (show_end_install_msg)
    Log (LOG_TIMESTAMP) << "Ending cygwin install" << endLog;

  for (FileSet::iterator i = files.begin();
       i != files.end(); ++i)
    {
      log_save (i->level, i->key, i->append);
    }
  // TODO: remove this when the ::exit issue is tidied up.
  ::exit (exit_code);
}

void
LogFile::flushAll ()
{
  Log (LOG_TIMESTAMP) << "Writing messages to log files without exiting" << endLog;

  for (FileSet::iterator i = files.begin();
       i != files.end(); ++i)
    {
      log_save (i->level, i->key, i->append);
    }
}

void
LogFile::log_save (int babble, const std::string& filename, bool append)
{
  static int been_here = 0;
  if (been_here)
    return;
  been_here = 1;

  io_stream::mkpath_p (PATH_TO_FILE, "file://" + filename, 0755);

  io_stream *f = io_stream::open("file://" + filename, append ? "at" : "wt", 0644);
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

std::ostream &
LogFile::operator() (log_level theLevel)
{
  if (theLevel < 1 || theLevel > 2)
    throw new std::invalid_argument("Invalid log_level");
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
  std::string buf = theStream->str();
  delete theStream;

  /* also write to stdout */
  if ((currEnt->level >= LOG_PLAIN) || VerboseOutput)
    {
      /*
        The log message is UTF-8 encoded. Re-encode this in the console output
        codepage (so it can be correctly decoded by a Windows terminal).
        Unfortunately there's no API for direct multibyte re-encoding, so we
        must do it in two steps UTF-8 -> UTF-16 -> CP_COCP.

        If the console output codepage is UTF-8, we already have the log message
        in the correct encoding, so we can avoid doing all that work.

        If the output is not a console, GetConsoleOutputCP() returns 0.
        Possibly it's a Cygwin pty?
      */
      std::string cpbuf = buf;

      unsigned int ocp = GetConsoleOutputCP();
      if ((ocp != 0 ) && (ocp != 65001))
        cpbuf = wstring_to_string(string_to_wstring(buf), ocp);

      std::cout << cpbuf << std::endl;
    }

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
  currEnt->msg += buf;

  /* reset for next use */
  theStream = new std::stringbuf;
  rdbuf (theStream);
  init (theStream);
}
