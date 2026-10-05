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
#include "LogFile.h"
#include "io_stream.h"
#include "msg.h"
#include "resource.h"
#include <iostream>
#include <sstream>
#include <set>
#include <time.h>
#include <string>
#include "String++.h"
#include "getopt++/BoolOption.h"

static BoolOption VerboseOutput (false, 'v', "verbose", IDS_HELPTEXT_VERBOSE);

// Global logger instance
static LogFile global_logger;

LogFile &getGlobalLogger()
{
  return global_logger;
}

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

typedef std::set<filedef> FileSet;
static FileSet files;

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

// LogStream Implementation
LogStream::LogStream(LogFile *parent, log_level level)
  : std::ostream(nullptr), parent_(parent), level_(level), buffer_(new std::stringbuf())
{
  rdbuf(buffer_);
}

LogStream::~LogStream()
{
  delete buffer_;
}

// LogFile Implementation
LogFile::LogFile()
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
LogFile::saveAll ()
{
  for (FileSet::iterator i = files.begin();
       i != files.end(); ++i)
    {
      log_save (i->level, i->key, i->append);
    }
}

void
LogFile::log_save (int minlevel, const std::string& filename, bool append)
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
      if (l->level >= minlevel)
        {
          char b[100];
          struct tm *tm = localtime (&(l->when));
          strftime (b, 100, "%Y/%m/%d %H:%M:%S ", tm);
          f->write(b, strlen(b));

          const char *tstr = l->msg.c_str();
          f->write (tstr, strlen (tstr));
          if (tstr[strlen (tstr) - 1] != '\n')
            f->write ("\n", 1);
        }
    }

  delete f;
  been_here = 0;
}

std::unique_ptr<LogStream>
LogFile::getStream(log_level theLevel)
{
  if (theLevel < 1 || theLevel > 2)
    throw new std::invalid_argument("Invalid log_level");

  return std::unique_ptr<LogStream>(new LogStream(this, theLevel));
}

void
LogFile::endEntry(LogStream &stream)
{
  std::stringbuf *strbuf = stream.getBuffer();
  if (!strbuf)
    return;

  std::string buf = strbuf->str();

  /* also write to stdout */
  if ((stream.getLevel() >= LOG_PLAIN) || VerboseOutput)
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

  LogEnt *currEnt = new LogEnt;
  currEnt->next = 0;
  currEnt->level = stream.getLevel();
  currEnt->msg = buf;
  time (&(currEnt->when));

  /* add to log entry chain */
  *next_logent = currEnt;
  next_logent = &(currEnt->next);
}

// End-of-log message stream manipulator
std::ostream& endLog(std::ostream& outs)
{
  // Cast to LogStream to access parent and level information.
  LogStream *log_stream = dynamic_cast<LogStream *>(&outs);
  if (log_stream && log_stream->getParent())
    {
      log_stream->getParent()->endEntry(*log_stream);
    }
  return outs;
}
