/*
 * Copyright (c) 2000, Red Hat, Inc.
 *
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 *
 *     A copy of the GNU General Public License can be found at
 *     http://www.gnu.org/
 *
 * Written by DJ Delorie <dj@redhat.com>
 *
 */

/* The purpose of this file is to centralize all the logging functions. */

#if 0
static const char *cvsid =
  "\n%%% $Id$\n";
#endif

#include "win32.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>

#include "resource.h"
#include "msg.h"
#include "log.h"
#include "dialog.h"
#include "state.h"
#include "mkdir.h"
#include "mount.h"

#include "io_stream.h"

struct LogEnt
{
  LogEnt *next;
  enum log_level level;
  time_t when;
  String msg;
};

static LogEnt *first_logent = 0;
static LogEnt **next_logent = &first_logent;

void 
log (enum log_level level, String const &message)
{
  LogEnt *l = new LogEnt;
  l->next = 0;
  l->level = level;
  time (&(l->when));
  *next_logent = l;
  next_logent = &(l->next);
  l->msg = message;

  char b[100];
  b[0]='\0';
  if (level == LOG_TIMESTAMP)
    {
      struct tm *tm = localtime (&(l->when));
      strftime (b, 1000, "%Y/%m/%d %H:%M:%S ", tm);
    }
  l->msg = String (b) + message;

  msg ("LOG: %d %s", l->level, l->msg.cstr_oneuse());
}

void
log (enum log_level level, const char *fmt, ...)
{
  char buf[1000];
  va_list args;
  va_start (args, fmt);
  vsprintf (buf, fmt, args);
  log (level, String(buf));
}

void
log_save (int babble, String const &filename, int append)
{
  static int been_here = 0;
  if (been_here)
    return;
  been_here = 1;

  io_stream::mkpath_p (PATH_TO_FILE, filename);

  io_stream *f = io_stream::open(String("file://")+filename, append ? "at" : "wt");
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

void
exit_setup (int exit_code)
{
  static int been_here = 0;
  if (been_here)
    ExitProcess (1);
  been_here = 1;

  if (exit_msg)
    note (NULL, exit_msg);

  log (LOG_TIMESTAMP, "Ending cygwin install");

  if (source == IDC_SOURCE_DOWNLOAD || !get_root_dir ().size())
    {
      log_save (LOG_BABBLE, local_dir + "/setup.log.full", 0);
      log_save (0, local_dir + "/setup.log", 1);
    }
  else
    {
      log_save (LOG_BABBLE, cygpath ("/var/log/setup.log.full"), 0);
      log_save (0, cygpath ("/var/log/setup.log"), 1);
    }

  ExitProcess (exit_code);
}
